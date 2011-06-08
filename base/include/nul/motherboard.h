/** @file
 * Virtual motherboard.
 *
 * Copyright (C) 2007-2010, Bernhard Kauer <bk@vmmon.org>
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
  class Hip   *_hip;

  /**
   * To avoid bugs we disallow the copy constuctor.
   */
  Motherboard(const Motherboard &bus) { Logging::panic("%s copy constructor called", __func__); }

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
  DBus<MessageInput>        bus_input;
  DBus<MessageIrq>          bus_hostirq;
  DBus<MessageIrq>          bus_irqlines;
  DBus<MessageIrqNotify>    bus_irqnotify;
  DBus<MessageLegacy>       bus_legacy;
  DBus<MessageMem>          bus_mem;
  DBus<MessageMemRegion>    bus_memregion;
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
  DBus<MessageVirtualNet>   bus_vnet;
  DBus<MessageVirtualNetPing> bus_vnetping;

  VCpu *last_vcpu;
  Clock *clock() { return _clock; }
  Hip   *hip() { return _hip; }

  /**
   * Parse the cmdline and create devices.
   */
  void parse_args(const char *args, const char * stop = 0)
  {
#define WORD_SEPARATOR " \t\r\n\f"
#define PARAM_SEPARATOR ",+"
    while (args[0]) {
      if (stop && !strncmp(stop, args, strlen(stop))) return;
      unsigned arglen = strcspn(args, WORD_SEPARATOR);
      if (!arglen) {
        args++;
        continue;
      }
      long *p = &__param_table_start;
      bool handled = false;
      while (!handled && p < &__param_table_end) {
        typedef void  (*CreateFunction)(Motherboard *, unsigned long *argv,
                                        const char *args, unsigned args_len);
        CreateFunction func = reinterpret_cast<CreateFunction>(*p++);
        char **strings = reinterpret_cast<char **>(*p++);

        unsigned prefixlen = strcspn(args, ":" WORD_SEPARATOR);
        if (strlen(strings[0]) == prefixlen && !memcmp(args, strings[0], prefixlen)) {
          Logging::printf("\t=> %.*s <=\n", arglen, args);

          const char *s = args + prefixlen;
          if (args[prefixlen] == ':') s++;
          const char *start = s;
          unsigned long argv[16];
          for (unsigned j=0; j < 16; j++) {
            unsigned alen = strcspn(s, PARAM_SEPARATOR WORD_SEPARATOR);
            if (alen)
              argv[j] = strtoul(s, 0, 0);
            else
              argv[j] = ~0UL;
            s+= alen;
            if (s[0] && strchr(PARAM_SEPARATOR, s[0])) s++;
          }
          func(this, argv, start, s - start);
          handled = true;
        }
      }
      if (!handled) Logging::printf("Ignored parameter: '%.*s'\n", arglen, args);
      args += arglen;
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

  Motherboard(Clock *__clock, Hip *__hip) : _clock(__clock), _hip(__hip), last_vcpu(0)  {}
};
