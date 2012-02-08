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

class ParentPerf : public WvProgram
{
  static const unsigned tries = 1000;
  uint64 results[tries];

  void benchmark(EchoProtocol *echo) {
    uint64 tic, tac, min = ~0ull, max = 0, ipc_duration;

    // Warmup call to wait on the service to start and get the cache warm
    echo->echo(*myutcb(), 42);
    echo->close();
    
    tic = Cpu::rdtsc();
    echo->echo(*myutcb(), 42);
    tac = Cpu::rdtsc();
    uint64 open_session = tac - tic;
    WVPERF(open_session, "cycles");
    
    for (unsigned i=0; i<tries; i++) {
      tic = Cpu::rdtsc();
      echo->echo(*myutcb(), 42);
      tac = Cpu::rdtsc();
      ipc_duration = tac - tic;
      min = MIN(min, ipc_duration);
      max = MAX(max, ipc_duration);
      results[i] = ipc_duration;
    }
    uint64 avg = 0;
    for (unsigned i=0; i<tries; i++)
      avg += results[i];

    avg = Math::muldiv128(avg, 1, tries);
    WVPERF(avg, "cycles");
    WVPERF(min, "cycles");
    WVPERF(max, "cycles");
  }
public:
  void wvrun(Utcb *utcb, Hip *hip)
  {
    WVSTART("Service without sessions");
    benchmark(new EchoProtocol(this, 0));
    WVSTART("Service with sessions");
    benchmark(new EchoProtocol(this, 1));
    WVSTART("Service with sessions (implemented as a subclass of SService)");
    benchmark(new EchoProtocol(this, 2));
  }
};

ASMFUNCS(ParentPerf, WvTest)
