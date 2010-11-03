/**
 * Measuring idle time.
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL.
 *
 * NUL is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 * License version 2 for more details.
 */

// Novatop uses a specially patched hypervisor to read the
// architectural fixed-function performance counters. Each column
// represents one CPU and displays the ratio between unhalted and
// halted clock cycles, i.e. the time the CPU did some work.

#include <nul/types.h>
#include <sigma0/console.h>
#include <nul/program.h>
#include <nul/motherboard.h>
#include <nul/service_timer.h>

static uint8 perfctl_init() { return nova_syscall(14, 0, 0, 0, 0); }
static uint8 perfctl_read() { return nova_syscall(14, 1, 0, 0, 0); }

enum {
  WIDTH  = 80,
  HEIGHT = 25,
};

template <typename T, unsigned SIZE>
class History
{
  unsigned _cur;
  T _buf[SIZE];

public:
  void add(T v)
  {
    _cur = (_cur + 1) % SIZE;
    _buf[_cur] = v;
  }

  T &operator[](unsigned i)
  {
    assert(i < SIZE);
    return _buf[(_cur + SIZE - i) % SIZE];
  }

  History() : _cur(0), _buf() { }
};

class IdleTest : public NovaProgram, public ProgramConsole
{
  Hip            *_hip;
  uint64          _cycles_per_second;
  unsigned       *_wait_sm;
  History<unsigned, WIDTH> *_history;


  __attribute__((regparm(0), noreturn)) static void
  idle_thread(void *t)
  {
    IdleTest *nt = reinterpret_cast<IdleTest *>(t);
    nt->idle_wait_loop();
  }

public:
  typedef unsigned long __attribute__((regparm(1))) (*pt_func)(unsigned, Utcb *);

  __attribute__((regparm(1)))
  static void do_thread_startup(unsigned m, unsigned eip)
  {
    Utcb *u = myutcb();
    u->eip = reinterpret_cast<unsigned *>(u->esp)[0];
  }

  __attribute__((noreturn)) void run(Utcb *utcb, Hip *hip)
  {
    console_init("IC");
    unsigned res;
    if ((res = init(hip))) Logging::panic("init failed with %x", res);
    init_mem(hip);
    _hip = hip;

    _cycles_per_second = static_cast<uint64>(hip->freq_tsc)*1000;
    _history           = new History<unsigned, WIDTH>[hip->cpu_count()];

    Logging::printf("IDLE up.\n%u CPUs, %llu Cycles/s\n",
		    hip->cpu_count(), _cycles_per_second);

    if (perfctl_init() != 0) Logging::panic("Unmodified NOVA.");
    Logging::printf("Modified NOVA. Using performance counters.\n");

    _wait_sm = new unsigned[hip->cpu_count()];

    for (unsigned i = 0; i < hip->cpu_count(); i++) {
      unsigned c = hip->cpu_physical(i);

      Logging::printf("Starting idle thread for CPU %u.\n", c);
      nova_create_sm(_wait_sm[i] = alloc_cap());
      Logging::printf("_wait_sm[%u] = %x\n", i, _wait_sm[i]);

      unsigned cap_off = 0x10000 + c*0x20;
      unsigned exc_ec = create_ec_helper(0xDEAD, 0, 0, c /* cpunr */);
      nova_create_pt(cap_off + 0x1e, exc_ec, reinterpret_cast<mword>(do_thread_startup), MTD_RSP | MTD_RIP_LEN);


      unsigned ec_cap = create_ec_helper(reinterpret_cast<mword>(this), 0, cap_off, c, reinterpret_cast<void *>(&idle_thread));

      if (0 != nova_create_sc(alloc_cap(), ec_cap, Qpd(4, 10000)))
	Logging::panic("SC");
    }

    Clock c(_hip->freq_tsc*1000);

    TimerProtocol *_timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_NUM));
    TimerProtocol::MessageTimer msg(c.abstime(0, 1000));
    if (_timer_service->timer(*utcb, msg))
      Logging::panic("setting timeout failed\n");

    KernelSemaphore sem = KernelSemaphore(_timer_service->get_notify_sm());

    while (1) {

      TimerProtocol::MessageTimer m(c.abstime(1, 1));
      if (_timer_service->timer(*utcb, m))
        Logging::panic("setting timeout failed\n");

      sem.downmulti();

      for (unsigned i = 0; i < hip->cpu_count(); i++)
        nova_semup(_wait_sm[i]);
    }
  }

  unsigned cpu_logical(unsigned physical)
  {
    for (unsigned i = 0; i < _hip->cpu_count(); i++)
      if (_hip->cpu_physical(i) == physical)
	return i;
    assert(false);
  }


  void plot(unsigned cpu, uint64 v1, uint64 v2)
  {
    unsigned our_width = WIDTH/_hip->cpu_count();
    unsigned our_off   = cpu*our_width;
    char d[WIDTH*2];
    memset(d, ' ', sizeof(d));
    Vprintf::snprintf(d+0,  WIDTH, "%16lld", v1);
    Vprintf::snprintf(d+WIDTH, WIDTH, "%16lld", v2);

    // Iterate over history values
    for (unsigned i = 0; i < our_width; i++) {
      unsigned c = i + our_off;
      unsigned h = HEIGHT - 4;	// We need two rows for stats.
      unsigned now = 2 + (h*_history[cpu][i]) / 100;
      unsigned left  = (i > 0) ? (2 + h*_history[cpu][i-1]/100) : now;
      unsigned right = 2 + h*_history[cpu][i+1]/100;

      // Draw bar
      for (unsigned r = 0; r < HEIGHT; r++) {
	uint8 *vga = reinterpret_cast<uint8 *>(_vga_console + 2*((HEIGHT - r -1)*WIDTH + c));
	if (i == (our_width-1)) {
	  vga[0] = '|';
	  vga[1] = 0x08;
	} else {
	  if (r >= HEIGHT - 2) {
	    vga[0] = d[(r + 2 - HEIGHT) * WIDTH + i];
	    vga[1] = 0x0F;
	  }
	  else if (r < 2) {
	    vga[0] = '0' + (_history[cpu][i] / ((r ? 10 : 1))) % 10;
	    vga[1] = 0x0F;
	  } else {
	    char ch = '*';

	    if (r == now) {
	      if (now == 2) { ch = '-'; goto draw; }
	      if ((left == now) and (right == now)) { ch = '-'; goto draw; }
	      if ((left<now) and (right<now)) { ch = '^'; goto draw; }
	      if ((left>now) and (right>now)) { ch = 'v'; goto draw; }
	      if (now>right) ch = '\\'; else ch = '/';
	    }

	  draw:
	    vga[0] = (r <= now) ? ch : ' ';
	    if (now < (HEIGHT/4))
	      vga[1] = 0x07;
	    else if (now < 2*(HEIGHT/4))
	      vga[1] = 0x0F;
	    else if (now < 3*(HEIGHT/4))
	      vga[1] = 0x0E;
	    else
	      vga[1] = 0x0C;
	  }
	}
      }	// bars
    } // history
  }

  __attribute__((noreturn)) void idle_wait_loop()
  {
    unsigned cpu = cpu_logical(Cpu::cpunr());
    uint64 tsc = 0;
    uint64 fc2 = 0;		// CPU_CLK_UNHALTED.REF. This counter
				// is guaranteed to count as fast as
				// TSC.
    uint64 old0 = 0;
    uint64 old1 = 0;

    uint64  *m = reinterpret_cast<uint64 *>(myutcb()->msg);

    perfctl_init();
    while (1) {
      nova_semdown(_wait_sm[cpu]);
      perfctl_read();

      uint64 total = m[0] - tsc;
      uint64 busy  = fc2;

      // If total is zero, we die. Something is b0rken then.
      _history[cpu].add((busy * 100) / total);

      tsc = m[0];
      fc2 = m[1];
      plot(cpu, m[2] - old0, m[3] - old1);
      old0 = m[2];
      old1 = m[3];
    }
  }
};

ASMFUNCS(IdleTest, NovaProgram)

// EOF
