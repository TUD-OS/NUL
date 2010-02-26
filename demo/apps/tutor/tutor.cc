/// -*- Mode: C++ -*-
/// Show a screenful of helpful tips.
///
/// Copyright (C) 2009, Julian Stecklina
//
/// This file is part of NUL, the NOVA userland. See LICENSE for
/// licensing details.

#include <nul/program.h>
#include <sigma0/console.h>


void print_screen1(unsigned short *dst);

class Tutor : public NovaProgram,
	      public ProgramConsole
{
  const char *debug_getname() { return "Tutor"; };

public:
  void run(Hip *hip)
  {
    console_init("TUT");

    init(hip);

    print_screen1(_console_data.screen_address);

    block_forever();

  }
};

ASMFUNCS(Tutor, NovaProgram);

// EOF
