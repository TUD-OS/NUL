/// -*- Mode: C++ -*-
/// Show a screenful of helpful tips.
///
/// Copyright (C) 2009-2010, Julian Stecklina
//
/// This file is part of NUL, the NOVA userland. See LICENSE for
/// licensing details.

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
    console_init("TUT");

    init(hip);

    // Get keyboard
    Logging::printf("Getting keyboard\n");
    KernelSemaphore sem(alloc_cap(), true);
    StdinConsumer stdinconsumer;
    Sigma0Base::request_stdin(utcb, &stdinconsumer, sem.sm());

    unsigned line = 0;

    print_screen1(_console_data.screen_address, line);
    while (1) {
      sem.downmulti();
      while (stdinconsumer.isData()) {
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
