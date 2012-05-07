/**
 * @file
 * Easy to use sleep() functionality
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

#include <nul/service_timer.h>
#include <nul/baseprogram.h>

class TimerHelper : public TimerProtocol
{
  Clock           clock;
  KernelSemaphore timersem;

public:

  unsigned msleep(unsigned msecs)
  {
    unsigned res;
    res = timer(*BaseProgram::myutcb(), clock.abstime(msecs, 1000));
    if (res) return res;
    timersem.downmulti();
    return ENONE;
  }

  TimerHelper(CapAllocator *a) :
    TimerProtocol(a->alloc_cap(TimerProtocol::CAP_SERVER_PT + Global::hip.cpu_desc_count())),
    clock(Global::hip.freq_tsc * 1000),
    timersem(get_notify_sm())

  {}
};
