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
    Clock *clock = new Clock(hip->freq_tsc * 1000);
    TimerProtocol *timer_service = new TimerProtocol(alloc_cap_region(TimerProtocol::CAP_SERVER_PT + hip->cpu_desc_count(), 0));

    WVPASS(clock);
    WVPASS(timer_service);

    timevalue t1, t2;
    timevalue ts;

    WVNUL(timer_service->time(*utcb, t1, ts));
    WVNUL(timer_service->timer(*utcb, clock->abstime(100, 1000))); // 100 ms
    KernelSemaphore timersem = KernelSemaphore(timer_service->get_notify_sm());

    timersem.downmulti();
    WVNUL(timer_service->time(*utcb, t2, ts));
    timevalue sleep_time_ms = (t2 - t1 /* us */) / 1000;

    WVPASSGE(static_cast<int>(sleep_time_ms), 100); // Broken in qemu
  }
};

ASMFUNCS(TimerTest, WvTest)
