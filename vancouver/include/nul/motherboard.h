/**
 * Virtual motherboard.
 *
 * Copyright (C) 2007-2010, Bernhard Kauer <bk@vmmon.org>
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
#include "service/helper.h"
#include "service/params.h"
#include "service/profile.h"
#include "service/string.h"
#include "config.h"
#include "bus.h"
#include "message.h"
#include "timer.h"
#include "templates.h"


class VCpu;

/**
 * A virtual motherboard is a collection of busses.
 * The devices are later attached to the busses.
 *
 * This also knows the backend devices.
 */
class Motherboard : public StaticReceiver<Motherboard>
{
  Clock *_clock;
 public:
  DBus<MessageAcpi>         bus_acpi;
  DBus<MessageAhciSetDrive> bus_ahcicontroller;
  DBus<MessageApic>         bus_apic;
  DBus<MessageBios>         bus_bios;
  DBus<MessageConsole>      bus_console;
  DBus<MessageDiscovery>    bus_discovery;
  DBus<MessageDisk>         bus_disk;
  DBus<MessageDiskCommit>   bus_diskcommit;
  DBus<MessageHostOp>       bus_hostop;
  DBus<MessageIOIn>         bus_hwioin;
  DBus<MessageIOIn>         bus_ioin;
  DBus<MessageIOOut>        bus_hwioout;
  DBus<MessageIOOut>        bus_ioout;
  DBus<MessageIrq>          bus_hostirq;
  DBus<MessageIrq>          bus_irqlines;
  DBus<MessageIrqNotify>    bus_irqnotify;
  DBus<MessageKeycode>      bus_keycode;
  DBus<MessageLegacy>       bus_legacy;
  DBus<MessageMem>          bus_mem;
  DBus<MessageMemRegion>    bus_memregion;
  DBus<MessageMouse>        bus_mouse;
  DBus<MessageNetwork>      bus_network;
  DBus<MessagePS2>          bus_ps2;
  DBus<MessagePciConfig>    bus_hwpcicfg;
  DBus<MessagePciConfig>    bus_pcicfg;
  DBus<MessagePic>          bus_pic;
  DBus<MessagePit>          bus_pit;
  DBus<MessageSerial>       bus_serial;
  DBus<MessageTime>         bus_time;
  DBus<MessageTimeout>      bus_timeout;
  DBus<MessageTimer>        bus_timer;
  DBus<MessageVesa>         bus_vesa;

  VCpu *last_vcpu;
  Clock *clock() { return _clock; }

  /**
   * Parse the cmdline and create devices.
   */
  void parse_args(char *args)
  {
    char *s;
    while ((s = strsep(&args, " \t\r\n\f")))
      {
	long *p = &__param_table_start;
	bool handled = false;
	while (!handled && s[0] && p < &__param_table_end)
	  {
	    typedef void  (*CreateFunction)(Motherboard *, unsigned long *argv);
	    CreateFunction func = reinterpret_cast<CreateFunction>(*p++);
	    char **strings = reinterpret_cast<char **>(*p++);

	    unsigned len = strlen(strings[0]);
	    if (!memcmp(s, strings[0], len))
	      {
		if (s[len] == ':')  len++;
		else if (s[len])    continue;

		Logging::printf("Handle parameter: '%s'\n", s);

		// skip prefix and colon
		s += len;
		if (s[0] && s[0] != ':')
		if (s[0] == ':') s++;
		unsigned long argv[16];
		for (unsigned j=0; j < 16; j++)
		  {
		    char *argvalue = strsep(&s, ",+");
		    if (argvalue && *argvalue)
		      argv[j] = strtoul(argvalue, 0, 0);
		    else
		      argv[j] = ~0UL;
		  }
		func(this, argv);
		handled = true;
	      };
	  }
	if (!handled && s[0]) Logging::printf("Ignored parameter: '%s'\n", s);
      }
  }


  /**
   * Dump the profiling counters.
   */
  void dump_counters(bool full = false)
  {
    static timevalue orig_time;
    timevalue t = _clock->clock(1000);
    COUNTER_SET("Time", t - orig_time);
    orig_time = t;

    Logging::printf("VMSTAT\n");

    extern long __profile_table_start, __profile_table_end;
    long *p = &__profile_table_start;
    while (p < &__profile_table_end)
      {
	char *name = reinterpret_cast<char *>(*p++);
	long v = *p++;
	if (v && ((v - *p) || full))
	  Logging::printf("\t%12s %8ld %8lx  diff %8ld\n", name, v, v, v - *p);
	*p++ = v;
      }
  }

  void *operator new(unsigned size)  { return new(0x1000) char[size]; }
  Motherboard(Clock *__clock) : _clock(__clock), last_vcpu(0)  {}
};
