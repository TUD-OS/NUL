// -*- Mode: C++ -*-
/** @file
 * Sinus plasma and some motivational quotes.
 *
 * Copyright (C) 2009, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <nul/program.h>
#include <nul/timer.h>
#include <nul/service_timer.h>
#include <sigma0/console.h>

#include "math.h"

EXTERN_C void dlmalloc_init(cap_sel pool);

#define SIN_LUTSIZE   (1<<8)
#define SQRT_LUTSIZE  (1<<16)
#define SQRT_PRESHIFT (2)
static int8_t  sinlut[SIN_LUTSIZE];
static uint8_t sqrtlut[SQRT_LUTSIZE >> SQRT_PRESHIFT];

static void
gen_sinlut(void)
{
  for (int i = 0; i < SIN_LUTSIZE/2; i++) {
    sinlut[i] = static_cast<int8_t>(fsin(M_PI*2*static_cast<float>(i)/SIN_LUTSIZE)*127);
    sinlut[i + SIN_LUTSIZE/2] = -sinlut[i];
  }
}

static void
gen_sqrtlut(void)
{
  for (int i = 0; i < (SQRT_LUTSIZE >> SQRT_PRESHIFT); i++)
    sqrtlut[i] = fsqrt(i << SQRT_PRESHIFT);
}

static int16_t lsin(uint8_t v) { return sinlut[v]; }
static int16_t lcos(uint8_t v) { return lsin(64 - v); }
static uint8_t lsqrt(uint16_t v) { return sqrtlut[v >> SQRT_PRESHIFT]; }
static int16_t sqr(int16_t x) { return x*x; }

static uint8_t distance(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  return lsqrt(sqr(x1-x2) + sqr(y1-y2));
}


template <unsigned ROW, unsigned COL>
class TextBuffer {
private:
  uint16_t _buffer[ROW*COL];
public:

  uint16_t &character(unsigned row, unsigned col)
  {
    return _buffer[row*COL + col];
  }

  void blt_from(TextBuffer<ROW, COL> *target)
  {
    memcpy(_buffer, target->_buffer, sizeof(_buffer));
  }

  void blt_to(uint16_t *target)
  {
    memcpy(target, _buffer, sizeof(_buffer));
  }

  void put_text(unsigned row, unsigned col, uint8_t attr, const char *str, int len = -1)
  {
    while ((len-- != 0) && *str != 0) {
      if (*str != ' ')
	character(row, col) = *str | (attr<<8);
      col++;
      str++;
    }
  }

  TextBuffer() {
    memset(_buffer, 0, sizeof(_buffer));
  }
};

template <unsigned ROW, unsigned COL>
class TextAnimator : public TextBuffer<ROW, COL> {

public:
  virtual void render(timevalue now) = 0;

};

template <unsigned ROW, unsigned COL>
class PlasmaAnimator : public TextAnimator<ROW, COL> {

  void
  plasma_put(unsigned row, unsigned col, int8_t color)
  {
    int icol = (static_cast<int>(color) + 128) >> 4;
    uint16_t coltab[8] = { ' ' | 0x0000, ' ' | 0x0000,
			   ':' | 0x0200, ':' | 0x0A00,
			   'o' | 0x0200, 'O' | 0x0200,
			   'O' | 0x0A00, 'Q' | 0x0A00 };
    uint16_t attr = coltab[(icol <= 8) ? (8 - icol) : (icol - 8)];
    if (icol <= 8)
      attr = (attr & 0x8FF) | 0x0100;

    this->character(row, col) = attr;
  }


public:

  void render(timevalue now)
  {
    unsigned t = now >> 25;

    // Double ROW to correct aspect ratio.
    for (unsigned rc = 0; rc < ROW*2; rc += 2)
      for (unsigned cc = 0; cc < COL; cc++) {
	int8_t v1 = lsin(distance(rc, cc, ROW*2/2, COL/2)*2 + 2*t);
	int8_t v2 = lsin(distance(rc, cc,
				  (lsin(t>>5)/2 + 60),
				  (lcos(t>>5)/2 + 60)));

	int8_t v3 = lsin(distance(rc, cc,
				  (lsin(-t*3)/2 + 64),
				  (lcos(-t*3)/2 + 64)));

	plasma_put(rc/2, cc, (v1 + v2 + v3)/3);
      }

    this->character(ROW-1, 0) = this->character(ROW-1, 0) & 0xFF00 | 'J';
    this->character(ROW-1, 1) = this->character(ROW-1, 0) & 0xFF00 | 'S';
  }

};

template <unsigned ROW, unsigned COL>
class QuoteAnimator : public TextAnimator<ROW, COL> {
  TextAnimator<ROW, COL> *_background;
  bool _start_init;
  timevalue _start;

public:

  void render(timevalue now)
  {
    const uint16_t text_bg_attr = 0x0800;

    _background->render(now);
    this->blt_from(_background);

    if (!_start_init) {
      _start = now - 400<<22;
      _start_init = true;
    }

    unsigned t = (static_cast<unsigned>((now - _start) >> 22)) % 10000;
    unsigned cycle = (static_cast<unsigned>((now - _start) >> 22)) / 10000;

    if (t < 800) {
      // 0-799: Bar in
      for (unsigned rc = 0; rc < ROW; rc++)
	for (unsigned cc = 0; cc < COL; cc++) {
	  if ((cc <= t/10) && (rc > (ROW/2 - 2)) && (rc < (ROW/2 + 2))) {
	    uint16_t &ch = this->character(rc, cc);
	    ch = ch & 0xFF | text_bg_attr;
	  }
	}
    } else if (t < 5000) {
      // 800-1290 Empty bar
      for (unsigned rc = 0; rc < ROW; rc++)
	for (unsigned cc = 0; cc < COL; cc++) {
	  if ((rc > (ROW/2 - 2)) && (rc < (ROW/2 + 2))) {
	    uint16_t &ch = this->character(rc, cc);
	    ch = ch & 0xFF | text_bg_attr;
	  }
	}
      // 1300-4999
      const char *msg[] = {
	// A collection of nice Perlisms.
	"Simplicity does not precede complexity, but follows it.",
	"If your computer speaks English, it was probably made in Japan.",
	"To understand a program you must become both the machine and the program.",
	"If a listener nods his head when you're explaining your program, wake him up.",
	// Alan Kay quotes
	"Perspective is worth 80 IQ points.",
	"A successful technology creates problems that only it can solve.",
	"Simple things should be simple. Complex things should be possible.",
	// "If you don't fail at least 90 percent of the time, you're not aiming high enough." // XXX Too long
	// Kent Pitman
      };
      unsigned msg_no = sizeof(msg) / sizeof(*msg);
      const char *cur_msg = msg[cycle % msg_no];
      unsigned msg_len = strlen(cur_msg);

      if ((t >= 1300) && (t < 4500)) {
	this->put_text(ROW/2, COL/2 - msg_len/2 - 1, 0x0F, cur_msg, (t-1300)/10);
      }
      if ((t >= 4500) && (t < 4600)) {
	this->put_text(ROW/2, COL/2 - msg_len/2 - 1, 0x07, cur_msg, (t-1300)/10);
      }
    } else {
      // Bar out
      for (unsigned rc = 0; rc < ROW; rc++)
	for (unsigned cc = 0; cc < COL; cc++) {
	  if ((cc >= (t - 5000)/10) && (rc > (ROW/2 - 2)) && (rc < (ROW/2 + 2))) {
	    uint16_t &ch = this->character(rc, cc);
	    ch = ch & 0xFF | text_bg_attr;
	  }
	}

    }

  }

  QuoteAnimator(TextAnimator<ROW, COL> *background)
    : _background(background), _start_init(false)
  { }
};


static const char *intro_text[] = {
#include "intro-text.inc"
};


static uint32_t
random()
{
  // Linear Congruential Generator
  static uint32_t a = 1664525;
  static uint32_t c = 1013904223;
  static uint32_t x = 1;

  x = a*x + c;

  return x;
}

class IntroAnimator : public TextAnimator<25, 80>
{
  bool _start_init;
  timevalue _start;
  bool _done;
public:
  bool done() const { return _done; }

  void render(timevalue now)
  {
    unsigned ROW = 25;
    unsigned COL = 80;


    if (!_start_init) {
      _start = now;
      _start_init = true;

      for (unsigned rc = 0; rc < ROW; rc++)
	for (unsigned cc = 0; cc < COL; cc++) {
	  uint32_t r = random() % (127 - 32) + 32;
	  character(rc, cc) = r | 0x0800;
	}
    } else {

      for (unsigned rc = 0; rc < ROW; rc++)
	for (unsigned cc = 0; cc < COL; cc++) {
	  unsigned target;
	  uint16_t &chara = character(rc, cc);

	  unsigned start_row = (ROW - sizeof(intro_text)/sizeof(*intro_text)) / 2;
	  if ((rc >= start_row) && (rc - start_row < sizeof(intro_text)/sizeof(*intro_text))
	      && (cc < strlen(intro_text[rc - start_row]))) {
	     target = intro_text[rc - start_row][cc];
	  } else {
	    target = ' ';
	  }

	    if ((chara & 0xFF) == target) {
	      chara = chara & 0xFF | 0x0F00;
	      continue;
	    }

	    chara++;
	    if ((chara & 0xFF) > 127)
	      chara = 0x0700 | 32;

	}

    }
  }

  IntroAnimator()
    : _start_init(false), _done(false)
  {}
};


class Cycleburner : public NovaProgram,
		    public ProgramConsole
{
  const char *debug_getname() { return "Cycleburner"; };

public:
  void __attribute__((noreturn)) run(Utcb *utcb, Hip *hip)
  {
    init(hip);
    init_mem(hip);
    console_init("CYC", new Semaphore(alloc_cap(), true));
    dlmalloc_init(alloc_cap(0x1000));

    // attach to timer service 
    TimerProtocol * timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));
    KernelSemaphore sem = KernelSemaphore(timer_service->get_notify_sm());

    gen_sinlut();
    gen_sqrtlut();

    Clock *clock = new Clock(hip->freq_tsc*1000);
    IntroAnimator *ia = new IntroAnimator;
    PlasmaAnimator<25, 80> *pa = new PlasmaAnimator<25, 80>;
    QuoteAnimator<25, 80> *qa = new QuoteAnimator<25, 80>(pa);

    timevalue starttime = clock->time();
    while (1) {
      timevalue now = clock->time();

      if (now - starttime < 10000000000ULL) {
	ia->render(now);
	ia->blt_to(_console_data.screen_address);
      } else {
	qa->render(now);
	qa->blt_to(_console_data.screen_address);
      }

      // Wait
      if (timer_service->timer(*utcb, clock->time() + 50000000))
        Logging::panic("setting the timeout failed\n");

      sem.downmulti();
    }

    block_forever();

  }
};

ASMFUNCS(Cycleburner, NovaProgram)

// EOF
