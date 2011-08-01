/**
 * @file
 * Test application for testing wvtest framework
 *
 * Copyright (C) 2011 Michal Sojka <sojka@os.inf.tu-dreden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.nova.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

// TODO: Add nul/ below
#include <wvprogram.h>

class Test : public WvProgram
{
public:
  void wvrun(Utcb *utcb, Hip *hip)
  {
    WVPASS(1);
    WVPASSEQ(1, 1);
    WVPASSNE(1, 2);
    WVPASSLT(1, 2);

    WVPASSEQ("hello", "hello");
    WVPASSNE("hello", "hello2");

    char *cmdline = reinterpret_cast<char *>(hip->get_mod(0)->aux);

    // Fail if you see "fail" on command line
    WVFAIL(strstr(cmdline, "fail"));
  }
};

ASMFUNCS(Test, WvTest)
