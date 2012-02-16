/**
 * @file
 * Performance test of parent/service protocol
 *
 * Copyright (C) 2011, 2012 Michal Sojka <sojka@os.inf.tu-dresden.de>
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

#include <wvprogram.h>
#include "service_echo.h"
#include "service_echo_noxlate.h"
#include <nul/motherboard.h>

unsigned test = 0;
bool v = true; // verbose flag

static const unsigned maxtries = 1000000;
unsigned tries = 1000;

PARAM_HANDLER(test) { test = argv[0]; }
PARAM_HANDLER(quiet) { v = false; }
PARAM_HANDLER(tries) { tries = v ? MIN(maxtries, argv[0]) : argv[0]; }

class ParentPerf : public WvProgram
{
  unsigned results[maxtries];

  template <class T>
  void benchmark(T *echo) {
    uint64 tic, tac, min = ~0ull, max = 0, ipc_duration;
    unsigned res;

    // Warmup call to wait on the service to start and get the cache warm
    if (v) {
      res = echo->echo(*myutcb(), 42);
      assert(res == 42U);
      echo->close();
      res = echo->echo(*myutcb(), 42);
      assert(res == 42U);
      echo->close();
    
      tic = Cpu::rdtsc();
      echo->echo(*myutcb(), 42);
      tac = Cpu::rdtsc();
      assert(res == 42U);
      uint64 open_session = tac - tic;
      WVPERF(open_session, "cycles");
    }
    
    for (unsigned i=0; i<tries; i++) {
      tic = Cpu::rdtsc();
      echo->echo(*myutcb(), 42);
      tac = Cpu::rdtsc();
      assert(res == 42U);
      if (v) {
        ipc_duration = tac - tic;
        min = MIN(min, ipc_duration);
        max = MAX(max, ipc_duration);
        results[i] = ipc_duration;
      }
    }
    if (v) {
      uint64 avg = 0;
      for (unsigned i=0; i<tries; i++)
        avg += results[i];

      avg = Math::muldiv128(avg, 1, tries);
      WVPERF(avg, "cycles");
      WVPERF(min, "cycles");
      WVPERF(max, "cycles");
    }
    echo->close();
  }

public:
  void wvrun(Utcb *utcb, Hip *hip)
  {
    Motherboard *mb = new Motherboard(new Clock(hip->freq_tsc*1000), hip);
    mb->parse_args(reinterpret_cast<const char *>(hip->get_mod(0)->aux));

    if (!test || test == 1) {
      if (v) WVSTART("Service without sessions");
      benchmark(new EchoProtocol(this, 0));
    }
    if (!test || test == 2) {
      if (v) WVSTART("Service with sessions");
      benchmark(new EchoProtocol(this, 1));
    }
    if (!test || test == 3) {
      if (v) WVSTART("Service with sessions (implemented as a subclass of SService)");
      benchmark(new EchoProtocol(this, 2));
    }
    if (!test || test == 4) {
      if (v) WVSTART("Service with sessions represented by portals (implemented as a subclass of NoXlateSService)");
      benchmark(new EchoProtocolNoXlate(this, 3));
    }
  }
};

ASMFUNCS(ParentPerf, WvTest)
