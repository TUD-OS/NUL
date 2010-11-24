/**
 * Minimalistic presentation tool.
 *
 * Copyright (C) 2009, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL.
 *
 * NUL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "sigma0/console.h"
#include "host/keyboard.h"
#include "nul/program.h"
#include <tinf.h>


struct pheader {
  char           magic[4];
  unsigned short width;
  unsigned short height;
  unsigned int   pages;
  unsigned int   offset[];
} NOVA_PACKED;

class Rocknshine : public NovaProgram, ProgramConsole, GenericKeyboard
{

  char *  _vesa_console;
  VgaRegs _vesaregs;
  ConsoleModeInfo _modeinfo;

  pheader *_header;
  unsigned *_scratch;


  void show_page(unsigned page)
  {
    unsigned int dest_len = sizeof(_scratch);

    unsigned char *compressed = _header->offset[page] + reinterpret_cast<unsigned char *>(_header);
    unsigned size = _header->offset[page+1] - _header->offset[page];

    Logging::printf("Compressed page %d at %p (offset %x, 0x%x bytes).\n",
		    page + 1, compressed, _header->offset[page], size);

    unsigned long long start_decompress = Cpu::rdtsc();
    int tinf_res = tinf_zlib_uncompress(_scratch, &dest_len,
					compressed, size);

    if (tinf_res != TINF_OK) {
      Logging::printf("tinf returned %d.\n", tinf_res);
      return;
    }

    assert(dest_len == _header->height * _header->width * 3); 

    unsigned long long start_display = Cpu::rdtsc();
    unsigned *cur_vesa = reinterpret_cast<unsigned *>(_vesa_console);
    unsigned *cur_scratch = _scratch;
    for (unsigned y=0; y < _header->height; y++)
      {
	if (_modeinfo.bpp == 24) {
	    memcpy(cur_vesa, cur_scratch,  _header->width * 3);
	    cur_scratch +=  _header->width * 3 / 4;
	}
	else if (_modeinfo.bpp == 32)
	  for (unsigned x = 0; x < _header->width; x += 4) {

	    // Load 4 packed pixels in 3 words
	    unsigned p1 = *cur_scratch++;
	    unsigned p2 = *cur_scratch++;
	    unsigned p3 = *cur_scratch++;

	    cur_vesa[x + 0] = p1 & 0xFFFFFF;
	    cur_vesa[x + 1] = (p1 >> 24) | ((p2 & 0xFFFF) << 8);
	    cur_vesa[x + 2] = (p2 >> 16) | ((p3 & 0xFF) << 16);
	    cur_vesa[x + 3] = p3 >> 8;
	  }
	cur_vesa += _modeinfo.bytes_per_scanline / 4;
      }
    unsigned long long end_cycles = Cpu::rdtsc();
    Logging::printf("Decompression: %10llu cycles\n", start_display - start_decompress);
    Logging::printf("Display:       %10llu cycles\n", end_cycles - start_display);
  };

public:
  int run(Utcb *utcb, Hip *hip)
  {
    console_init("RS");
    unsigned res;
    if ((res = init(hip))) Logging::panic("init failed with %x", res);
    init_mem(hip);
    Logging::printf("rocknshine up and running\n");

    // Look for an module
    extern char __image_end;
    char *pmem = &__image_end;
    MessageHostOp msg1(1, pmem);
    if (Sigma0Base::hostop(msg1))
      Logging::panic("no enough memory for the module");

    _header = reinterpret_cast<pheader *>(pmem);
    _free_phys.del(Region(reinterpret_cast<unsigned long>(pmem), msg1.size));
    if (memcmp(_header->magic, "PRE0", sizeof(_header->magic)) != 0)
      Logging::panic("invalid module");

    Logging::printf("Loaded presentation: (%d, %d), %d page%s.\n",
		    _header->width, _header->height, _header->pages,
		    (_header->pages == 1) ? "" : "s");


    // Initialize tiny zlib library.
    tinf_init();
    Logging::printf("tinf library initialized.\n");

    unsigned mode = ~0u;

    // we like to have a 24/32bit mode but prefer a 24bit mode
    ConsoleModeInfo m;
    for (MessageConsole msg(0, &m); !Sigma0Base::console(msg); msg.index++)
      if (m.attr & 0x80
	  && m.bpp >= 24
	  && m.resolution[0] == _header->width
	  && m.resolution[1] >= _header->height
	  && ((mode == ~0u) || (_modeinfo.bpp > m.bpp)))
	{
	  mode = msg.index;
	  _modeinfo = m;
	}

    if (mode == ~0u) Logging::panic("have not found any 24/32bit graphic mode");
    unsigned size = _modeinfo.resolution[1] * _modeinfo.bytes_per_scanline;
    _scratch      = reinterpret_cast<unsigned *>(_free_phys.alloc(3 * _header->width * _header->height, 12));
    _vesa_console = reinterpret_cast<char *>(_free_phys.alloc(size, 12));
    if (!_scratch || !_vesa_console) Logging::panic("not enough memory - %ld MB should be sufficient", ((3 * _header->width * _header->height + size + msg1.size) >> 20) + 2);
    Logging::printf("RS: use %x %dx%d-%d %p size %x sc %x\n",
		    mode, _modeinfo.resolution[0], _modeinfo.resolution[1], _modeinfo.bpp, _vesa_console, size, _modeinfo.bytes_per_scanline);

    MessageConsole msg2("RS2", _vesa_console, size, &_vesaregs);
    check1(1, Sigma0Base::console(msg2));
    _vesaregs.mode = mode;

    // Get keyboard
    Logging::printf("Getting keyboard\n");
    KernelSemaphore sem = KernelSemaphore(alloc_cap(), true);
    StdinConsumer stdinconsumer;
    Sigma0Base::request_stdin(utcb, &stdinconsumer, sem.sm());

    // switch to our view
    msg2.type = MessageConsole::TYPE_SWITCH_VIEW;
    msg2.view = 1;
    Sigma0Base::console(msg2);

    unsigned last_page = 1;     // Force redraw
    unsigned page = 0;
    unsigned input = 0;

    while (1) {
      if (last_page != page) show_page(page);
      last_page = page;

      sem.downmulti();
      while (stdinconsumer.has_data()) {
        MessageInput *kmsg = stdinconsumer.get_buffer();
        switch (kmsg->data & ~KBFLAG_NUM) {
        case KBCODE_SPACE:
        case KBCODE_DOWN:
        case KBCODE_RIGHT:
          page = (page+1) % _header->pages;
          break;
        case KBCODE_BSPACE:
        case KBCODE_UP:
        case KBCODE_LEFT:
          page = (page + _header->pages - 1) % _header->pages;
          break;
        case KBCODE_HOME:
          page = 0;
          break;
        case KBCODE_END:
          page = _header->pages - 1;
          break;
        case KBCODE_ENTER:
        	if (input && input <= _header->pages) page = input - 1;
        case KBCODE_ESC:
        	input = 0;
        	break;
        case KBCODE_SCROLL:
        	for (unsigned i=0; i < _header->pages; i++)
          {
      	    page = (page + 1) % _header->pages;
    	      show_page(page);
      	  }
        	break;
        default:
        	{
        	  unsigned num;
        	  if ((num = is_numeric_key(kmsg->data, 0)))  input = input*10 + (num % 10);
        	}
        }
        stdinconsumer.free_buffer();
      }
    }
  }
};

ASMFUNCS(Rocknshine, NovaProgram)
