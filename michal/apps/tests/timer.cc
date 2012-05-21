/**
 * @file
 * Test application for disk service.
 *
 * Copyright (C) 2011 Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Copyright (C) 2012 Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2012 Alexander Boettcher <boettcher@tudos.org>
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

    timevalue t1, t2, t1_tsc, t2_tsc;
    timevalue ts;

    WVNUL(timer_service->time(*utcb, t1, ts));
    t1_tsc = clock->time();
    WVNUL(timer_service->timer(*utcb, clock->abstime(1, 1))); // 1s
    KernelSemaphore timersem = KernelSemaphore(timer_service->get_notify_sm());

    timersem.downmulti();
    t2_tsc = clock->time();
    WVNUL(timer_service->time(*utcb, t2, ts));
    timevalue sleep_time_us_service = (t2 - t1 /* us */);
    timevalue sleep_time_us_tsc     = clock->clock(1000000, t2_tsc - t1_tsc);

    Logging::printf("Slept: service->time()=%lluus, tsc_time=%lluus, tsc_freq=%u \n", sleep_time_us_service, sleep_time_us_tsc, hip->freq_tsc);
    WVPASSGE(static_cast<int>(sleep_time_us_service), 1000000); // Broken in qemu
    //WVPASSGE(static_cast<int>(sleep_time_us_tsc), 1000000); // Broken in qemu //drift to large between service(HPET) and tsc
  }
};

ASMFUNCS(TimerTest, WvTest)
