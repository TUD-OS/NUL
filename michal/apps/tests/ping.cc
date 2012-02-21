/**
 * @file 
 * Ping-part of cross-PD ping-pong benchmark
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

class Ping : public WvProgram
{
  static const unsigned tries = 1000;
  static unsigned counter;
  static void portal_func(unsigned long mtr) { counter++; }

  uint64 results[tries];
public:
  void wvrun(Utcb *utcb, Hip *hip)
  {
    cap_sel pseudonym = alloc_cap();
    cap_sel pt = alloc_cap();
    unsigned res;
    WVNUL(ParentProtocol::get_pseudonym(*utcb, "pong", 0, pseudonym));
    do {
      WV(res = ParentProtocol::get_portal(*utcb, pseudonym, pt, true));
    } while (res == ERETRY);
    WVNUL(res);
    
    uint64 tic, tac, xpd_min = ~0ull, xpd_max = 0, ipc_duration, rdtsc;
    tic = Cpu::rdtsc();
    tac = Cpu::rdtsc();
    rdtsc = tac-tic;
    for (unsigned i=0; i<tries; i++) {
      tic = Cpu::rdtsc();
      char res = nova_call(pt);
      tac = Cpu::rdtsc();
      assert(res == 0);
      ipc_duration = tac - tic - rdtsc;
      xpd_min = MIN(xpd_min, ipc_duration);
      xpd_max = MAX(xpd_max, ipc_duration);
      results[i] = ipc_duration;
    }
    uint64 xpd_avg = 0;
    for (unsigned i=0; i<tries; i++)
      xpd_avg += results[i];

    xpd_avg = Math::muldiv128(xpd_avg, 1, tries);
    WVPERF(xpd_avg, "cycles");
    WVPERF(xpd_min, "cycles");
    WVPERF(xpd_max, "cycles");
  }
};

ASMFUNCS(Ping, WvTest)
