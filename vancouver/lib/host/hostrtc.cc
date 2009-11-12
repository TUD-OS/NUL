/**
 * HostRtc driver.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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
#include "vmm/motherboard.h"
#include "vmm/math.h"
#include "libc/time.h"

/**
 * Readout the current time+date from the RTC.
 *
 * State: stable
 * Documentation: rtc82885.pdf, MC146818AS.pdf, Intel ICH{3-7} documentation
 */
class HostRtc : public StaticReceiver<HostRtc>
{
  #include "host/simplehwioin.h"
  #include "host/simplehwioout.h"

  enum {
    MS_TIMEOUT = 2000, // milliseconds
  };
  unsigned _iobase;
  timevalue _wallclocktime;
  timevalue _timestamp;

  const char *debug_getname() {  return "HostRTC"; }
  void debug_dump() {  
    Device::debug_dump();
    Logging::printf(" iobase %x", _iobase);
  };

  unsigned char rtc_read (unsigned index)  {  outb(index, _iobase);  return inb(_iobase + 1);  };
public:

  bool  receive(MessageTime &msg)
  {
    msg.wallclocktime = _wallclocktime;
    msg.timestamp = _timestamp;
    return true;
  }

  HostRtc(DBus<MessageIOIn> &bus_hwioin, DBus<MessageIOOut> &bus_hwioout, Clock *clock, unsigned iobase)
    : _bus_hwioin(bus_hwioin), _bus_hwioout(bus_hwioout), _iobase(iobase)
  {

    if (rtc_read(0xb) & 0x80) Logging::panic("RTC does not count (SET==1)!");
    
    /**
     * We wait for an update to happen to get a more accurate timing.
     *
     * Instead of triggering on the UIP flag, which is typically
     * enabled for less than 2ms, we wait on the Update-Ended-Flag
     * which is set on the falling edge of the UIP flag and sticky
     * until read out.
     */
    rtc_read(0xc); // clear it once
    timevalue now = clock->clock(1000);
    unsigned char flags = 0;
    do
      flags |= rtc_read(0xc);
    while (now + MS_TIMEOUT >= clock->clock(1000) && ~flags & 0x10);
    if (~flags & 0x10) Logging::printf("no RTC update within %d milliseconds! - patch Qemu/Bochs/Xen...\n", MS_TIMEOUT);
    
    // we keep the time when we read-out the RTC to only read it once
    _timestamp = clock->clock(MessageTime::FREQUENCY);

    // read out everything
    unsigned char data[14];
    for (unsigned i=0; i < sizeof(data); i++)
	data[i] = rtc_read(i);
    // Read the century from the IBM PC location
    // we put it temporarly in the unused weekday field
    data[6] = rtc_read(0x32);

    // convert twelve hour format
    if (~data[0xb] & 2)
      {
	unsigned char hour = data[4] & 0x7f;
	if (~data[0xb] & 4) Math::from_bcd(hour);
	hour %= 12;
	if (data[4] & 0x80)  hour += 12;
	if (~data[0xb] & 4) Math::to_bcd(hour);
	data[4] = hour;
      }

    // convert from BCD to binary
    if (~data[0xb] & 4)
      for (unsigned i=0; i < sizeof(data) && i < 10; i++) 
	Math::from_bcd(data[i]);


    // Convert to seconds since 1970. 
    int year = data[9] + 100*data[6];
    if (year < 1970)  year += 100;
    tm_simple time = tm_simple(year, data[8], data[7], data[4], data[2], data[0]);
    _wallclocktime = mktime(&time) * MessageTime::FREQUENCY;
    Logging::printf("RTC date: %d.%02d.%02d %d:%02d:%02d msec %lld\n", time.mday, time.mon, time.year, time.hour, time.min, time.sec, _wallclocktime);
  }
};


PARAM(hostrtc,
      {
	unsigned short iobase = argv[0] == ~0ul ? 0x70 : argv[0];
	MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, (iobase << 8) |  1);
	if (!mb.bus_hostop.send(msg1))
	  Logging::panic("%s failed to allocate ports %x+2\n", __PRETTY_FUNCTION__, iobase);

	Device *dev = new HostRtc(mb.bus_hwioin, mb.bus_hwioout, mb.clock(), iobase);
	mb.bus_time.add(dev, HostRtc::receive_static<MessageTime>);
      },
      "hostrtc:hostiobase=0x70 - use the host RTC as wall clock time backend.",
      "Example: 'hostrtc'.");
