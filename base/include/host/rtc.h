/** @file
 * HostRtc generic code
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

#pragma once

#include <service/time.h>
#include <service/bcd.h>

class BasicRtc {
protected:
#include <host/simplehwioin.h>
#include <host/simplehwioout.h>


  unsigned _iobase;

  enum {
    MS_TIMEOUT = 2000, // milliseconds
  };


  unsigned char rtc_read (unsigned index)  {  outb(index, _iobase);  return inb(_iobase + 1);  };

public:

  void rtc_sync(Clock *clock)
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
  }

  /**
   * Return seconds*FREQUENCY since epoch.
   */
  timevalue rtc_wallclock()
  {
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
	if (~data[0xb] & 4) Bcd::from_bcd(hour);
	hour %= 12;
	if (data[4] & 0x80)  hour += 12;
	if (~data[0xb] & 4) Bcd::to_bcd(hour);
	data[4] = hour;
      }

    // convert from BCD to binary
    if (~data[0xb] & 4)
      for (unsigned i=0; i < sizeof(data) && i < 10; i++)
	Bcd::from_bcd(data[i]);


    // Convert to seconds since 1970.
    int year = data[9] + 100*data[6];
    if (year < 1970)  year += 100;
    tm_simple time = tm_simple(year, data[8], data[7], data[4], data[2], data[0]);
    return mktime(&time) * MessageTime::FREQUENCY;
  }

  BasicRtc(DBus<MessageHwIOIn> &bus_hwioin, DBus<MessageHwIOOut> &bus_hwioout, unsigned iobase)
    : _bus_hwioin(bus_hwioin), _bus_hwioout(bus_hwioout), _iobase(iobase)
  {
    

  }

};

/* EOF */
