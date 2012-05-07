/** @file
 * Client part of the timer protocol
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2010, Alexander Boettcher <boettcher@tudos.org>
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
#pragma once

#include <nul/config.h>
#include <nul/parent.h>
#include <nul/generic_service.h>
#include <nul/timer.h>

struct TimerProtocol : public GenericProtocol {
  /**
   * Timer infrastructure.
   *
   * There is no frequency and clock here, as all is based on the same
   * clocksource.
   */

  /* Resolution of the wallclock. In ticks per second. */
  static const unsigned long WALLCLOCK_FREQUENCY = Config::WALLCLOCK_FREQUENCY;

  struct MessageTimer
  {
    timevalue abstime;
    MessageTimer(timevalue _abstime) : abstime(_abstime) {}
  };

  struct MessageTime
  {
    timevalue wallclocktime;
    timevalue timestamp;
    MessageTime() :  wallclocktime(0), timestamp(0) {}
  };

  enum {
    TYPE_REQUEST_TIMER = ParentProtocol::TYPE_GENERIC_END,
    TYPE_REQUEST_TIME,
    TYPE_REQUEST_LAST_TIMEOUT,
  };

  unsigned triggered_timeouts(Utcb &utcb, unsigned &count) {
    unsigned res = call_server(init_frame(utcb, TYPE_REQUEST_LAST_TIMEOUT), false);
    if (!res)
      utcb >> count;

    utcb.drop_frame();
    return res;
  }

  /**
   * Returns the wall clock time in microseconds.
   *
   * It also contains a timestamp of the Motherboard clock in
   * microseconds, to be able to adjust to the time already passed and
   * to detect out-of-date values.
   */
  unsigned time(Utcb &utcb, timevalue &wallclocktime, timevalue &timestamp) {
    MessageTime m;
    unsigned res = call_server(init_frame(utcb, TYPE_REQUEST_TIME), false);
    if (!res)
      utcb >> m;
    wallclocktime = m.wallclocktime;
    timestamp     = m.timestamp;
    utcb.drop_frame();
    return res;
  }

  /**
   * Program timer for timer.abstime
   */
  unsigned timer(Utcb &utcb, timevalue abstime) {
    MessageTimer t(abstime);
    return call_server(init_frame(utcb, TYPE_REQUEST_TIMER) << t, true);
  }

  TimerProtocol(unsigned cap_base, unsigned instance=0) : GenericProtocol("timer", instance, cap_base, true) {}
};
