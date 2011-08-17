/** @file
 * HostRtc driver.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "nul/motherboard.h"
#include "service/math.h"
#include "service/time.h"

#include <host/rtc.h>

/**
 * Readout the current time+date from the RTC.
 *
 * State: stable
 * Documentation: rtc82885.pdf, MC146818AS.pdf, Intel ICH{3-7} documentation
 */
class HostRtc : public StaticReceiver<HostRtc>,
                private BasicRtc
{
  timevalue _wallclocktime;
  timevalue _timestamp;

public:

  bool  receive(MessageTime &msg)
  {
    msg.wallclocktime = _wallclocktime;
    msg.timestamp = _timestamp;
    return true;
  }

  HostRtc(DBus<MessageHwIOIn> &bus_hwioin, DBus<MessageHwIOOut> &bus_hwioout, Clock *clock, unsigned iobase)
    : BasicRtc(bus_hwioin, bus_hwioout, iobase)
  {

    rtc_sync(clock);

    // we keep the time when we read-out the RTC to only read it once
    _wallclocktime = rtc_wallclock();
    _timestamp = clock->clock(MessageTime::FREQUENCY);

    tm_simple time;
    gmtime(_wallclocktime, &time);
    Logging::printf("RTC date: %d.%02d.%02d %d:%02d:%02d msec %lld\n", time.mday, time.mon, time.year, time.hour, time.min, time.sec, _wallclocktime);
  }
};


PARAM_HANDLER(hostrtc,
	      "hostrtc:hostiobase=0x70 - use the host RTC as wall clock time backend.",
	      "Example: 'hostrtc'.")
{
  unsigned short iobase = argv[0] == ~0ul ? 0x70 : argv[0];
  MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, (iobase << 8) |  1);
  if (!mb.bus_hostop.send(msg1))
    Logging::panic("%s failed to allocate ports %x+2\n", __PRETTY_FUNCTION__, iobase);

  HostRtc *dev = new HostRtc(mb.bus_hwioin, mb.bus_hwioout, mb.clock(), iobase);
  mb.bus_time.add(dev, HostRtc::receive_static<MessageTime>);
}
