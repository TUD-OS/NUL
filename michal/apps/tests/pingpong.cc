/**
 * @file 
 * Ping pong benchmark
 *
 * Copyright (C) 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <wvprogram.h>

class PingPong : public WvProgram
{
  static const unsigned tries = 1000;
  static unsigned counter;
  static void portal_func(unsigned long mtr) { counter++; }

  uint64 results[tries];
public:
  void wvrun(Utcb *utcb, Hip *hip)
  {
    cap_sel ec;
    WVPASS(ec = create_ec4pt(this, utcb->head.nul_cpunr, 0));
    WVSHOWHEX(ec);
    cap_sel pt = alloc_cap();
    WVNOVA(nova_create_pt(pt, ec, reinterpret_cast<unsigned long>(portal_func), 0));
    uint64 tic, tac, min = ~0ull, max = 0, ipc_duration, rdtsc;
    tic = Cpu::rdtsc();
    tac = Cpu::rdtsc();
    rdtsc = tac-tic;
    for (unsigned i=0; i<tries; i++) {
      tic = Cpu::rdtsc();
      char res = nova_call(pt);
      tac = Cpu::rdtsc();
      assert(res == 0);
      ipc_duration = tac - tic - rdtsc;
      min = MIN(min, ipc_duration);
      max = MAX(max, ipc_duration);
      results[i] = ipc_duration;
    }
    uint64 avg = 0;
    for (unsigned i=0; i<tries; i++)
      avg += results[i];

    avg = Math::muldiv128(avg, 1, tries);
    WVPASSEQ(counter, 1000u);
    WVPERF(avg, "cycles");
    WVPERF(min, "cycles");
    WVPERF(max, "cycles");
  }
};

unsigned PingPong::counter = 0;

ASMFUNCS(PingPong, WvTest)
