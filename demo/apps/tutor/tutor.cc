// -*- Mode: C++ -*-
/** @file
 * Show a screenful of helpful tips.
 *
 * Copyright (C) 2009-2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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
#include <host/keyboard.h>
#include <sigma0/console.h>

void print_screen1(unsigned short *dst, unsigned &line);

struct Tutor : public NovaProgram,
	       public ProgramConsole,
	       public GenericKeyboard
{
  void __attribute__((noreturn)) run(Utcb *utcb, Hip *hip)
  {
    init(hip);
    init_mem(hip);
    console_init("TUT", new Semaphore(alloc_cap(), true));

    // Get keyboard
    Logging::printf("Getting keyboard\n");
    KernelSemaphore sem(alloc_cap(), true);
    StdinConsumer stdinconsumer;
    Sigma0Base::request_stdin(utcb, &stdinconsumer, sem.sm());

    unsigned line = 0;

    print_screen1(_console_data.screen_address, line);
    while (1) {
      sem.downmulti();
      while (stdinconsumer.has_data()) {
        MessageInput *kmsg = stdinconsumer.get_buffer();
        switch (kmsg->data & ~KBFLAG_NUM) {
        case KBCODE_UP:    if (line > 0) line -= 1;
        case KBCODE_DOWN:  line +=  1; 
        case KBCODE_SPACE: line += 24;
          print_screen1(_console_data.screen_address, line);
          break;
        default:
          break;
        }
        stdinconsumer.free_buffer();
      }
    }
    
    // Not reached.
  }
};

ASMFUNCS(Tutor, NovaProgram)

// EOF
