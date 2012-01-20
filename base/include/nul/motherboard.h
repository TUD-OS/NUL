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
 * The devices are later attached to the busses. To find out what the
 * individual busses are good for, check the documentation of the
 * message classes.
 *
 * This also knows the backend devices.
 */
class Motherboard : public StaticReceiver<Motherboard>
{
  Clock *_clock;
  class Hip   *_hip;

  /**
   * To avoid bugs we disallow the copy constructor.
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
  DBus<MessageHwIOIn>       bus_hwioin;	    ///< HW I/O space reads
  DBus<MessageIOIn>         bus_ioin;       ///< I/O space reads from virtual machines
  DBus<MessageHwIOOut>      bus_hwioout;    ///< HW I/O space writes
  DBus<MessageIOOut>        bus_ioout;	    ///< I/O space writes from virtual machines
  DBus<MessageInput>        bus_input;
  DBus<MessageIrq>          bus_hostirq;    ///< Host IRQs
  DBus<MessageIrqLines>	    bus_irqlines;   ///< Virtual IRQs before they reach (virtual) IRQ controller
  DBus<MessageIrqNotify>    bus_irqnotify;
  DBus<MessageLegacy>       bus_legacy;
  DBus<MessageMem>          bus_mem;	    ///< Access to memory from virtual devices
  DBus<MessageMemRegion>    bus_memregion;  ///< Access to memory pages from virtual devices
  DBus<MessageNetwork>      bus_network;
  DBus<MessagePS2>          bus_ps2;
  DBus<MessageHwPciConfig>  bus_hwpcicfg;   ///< Access to real HW PCI configuration space
  DBus<MessagePciConfig>    bus_pcicfg;	    ///< Access to PCI configuration space of virtual devices
  DBus<MessagePic>          bus_pic;
  DBus<MessagePit>          bus_pit;
  DBus<MessageSerial>       bus_serial;
  DBus<MessageTime>         bus_time;
  DBus<MessageTimeout>      bus_timeout;    ///< Timer expiration notifications 
  DBus<MessageTimer>        bus_timer;      ///< Request for timers
  DBus<MessageVesa>         bus_vesa;

  VCpu *last_vcpu;
  Clock *clock() { return _clock; }
  Hip   *hip() { return _hip; }

  /* Argument parsing */

  static const char *word_separator()      { return " \t\r\n\f"; }
  static const char *param_separator()     { return ",+"; }
  static const char *wordparam_separator() { return ":"; }

  /**
   * Return a pointer to the next token in args and advance
   * args. Returns the length of the token in len.
   */
  static const char *next_arg(const char *&args, size_t &len)
  {
    len = 0;
    if (args[0] == 0) return 0;

    len = strcspn(args, word_separator());
    if (len == 0) {
      args += 1;
      return next_arg(args, len);
    } else {
      args += len;
      return args - len;
    }
  }

  void parse_args(const char *args, size_t length)
  {
    char buf[length+1];
    buf[length] = 0;
    memcpy(buf, args, length);
    parse_args(buf);
  }

  /**
   * Parse the cmdline and create devices.
   */
  void parse_args(const char *args)
  {
    const char *current;
    size_t      arglen;

    while ((current = next_arg(args, arglen))) {
      assert(arglen > 0);

      long *p = &__param_table_start;
      bool handled = false;
      while (!handled && p < &__param_table_end) {
        typedef void  (*CreateFunction)(Motherboard *, unsigned long *argv,
                                        const char *args, unsigned args_len);
        CreateFunction func = reinterpret_cast<CreateFunction>(*p++);
        char **strings = reinterpret_cast<char **>(*p++);

        unsigned prefixlen = MIN(strcspn(current, word_separator()),
                                 strcspn(current, wordparam_separator()));
        if (strlen(strings[0]) == prefixlen && !memcmp(current, strings[0], prefixlen)) {
          Logging::printf("\t=> %.*s <=\n", arglen, current);

          const char *s = current + prefixlen;
          if (s[0] && strcspn(current + prefixlen, wordparam_separator()) == 0) s++;
          const char *start = s;
          unsigned long argv[16];
          for (unsigned j=0; j < 16; j++) {
            unsigned alen = MIN(strcspn(s, param_separator()),
                                strcspn(s, word_separator()));
            if (alen)
              argv[j] = strtoul(s, 0, 0);
            else
              argv[j] = ~0UL;
            s+= alen;
            if (s[0] && strchr(param_separator(), s[0])) s++;
          }
          func(this, argv, start, s - start);
          handled = true;
        }
      }
      if (!handled) Logging::printf("Ignored parameter: '%.*s'\n", arglen, current);
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
