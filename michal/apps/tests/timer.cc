/**
 * @file
 * Test application for disk service.
 *
 * Copyright (C) 2011 Michal Sojka <sojka@os.inf.tu-dresden.de>
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
#include <nul/service_timer.h>

class TimerTest : public WvProgram
{
public:
  void wvrun(Utcb *utcb, Hip *hip)
  {
    Clock *clock = new Clock((hip->freq_tsc + 1) * 1000); //freq_tsc is rounded down, add 1KHz to program it not before our intended point in time
    TimerProtocol *timer_service = new TimerProtocol(alloc_cap_region(TimerProtocol::CAP_SERVER_PT + hip->cpu_desc_count(), 0));

    WVPASS(clock);
    WVPASS(timer_service);

    // Give the system a bit time to settle
    WVNUL(timer_service->timer(*utcb, clock->abstime(1, 1))); // 1s
    KernelSemaphore timersem = KernelSemaphore(timer_service->get_notify_sm());
    timersem.downmulti();


    timevalue t1, t2;

    t1 = clock->clock(1000000);
    WVNUL(timer_service->timer(*utcb, clock->abstime(1, 1))); // 1s
    timersem.downmulti();
    t2 = clock->clock(1000000);
    timevalue sleep_time_us = t2 - t1;

    Logging::printf("Slept %lluus\n", sleep_time_us);
    WVPASSGE(static_cast<int>(sleep_time_us), 1000000); // Broken in qemu
  }
};

ASMFUNCS(TimerTest, WvTest)
