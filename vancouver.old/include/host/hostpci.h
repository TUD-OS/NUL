/**
 * PCI helper functions for PCI drivers.
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

#pragma once

#include <vmm/motherboard.h>
#include <nova/cpu.h>

/**
 * A helper for PCI config space access.
 */
class HostPci
{
  DBus<MessagePciCfg> _bus_pcicfg;

 public:

  enum {
    BAR0 = 0x10,
    BAR1 = 0x14,
    
    MAX_BAR  = 6,
    BAR_SIZE = 4,
  };

  enum {
    BAR_PREFETCH  = 0x4,

    BAR_TYPE_MASK = 0x6,
    BAR_TYPE_32B  = 0x0,
    BAR_TYPE_64B  = 0x4,
    BAR_TYPE_1MB  = 0x2,

    BAR_IO        = 0x1,
    BAR_IO_MASK   = 0xFFFFFFFCU,
    BAR_MEM_MASK  = 0xFFFFFFF0U,
  };

  unsigned conf_read(unsigned address)
  {
    MessagePciCfg msg(address);
    _bus_pcicfg.send(msg);
    return msg.value;
  }

  void conf_write(unsigned address, unsigned value)
  {
    MessagePciCfg msg(address, value);
    _bus_pcicfg.send(msg);
  }

  /** Determines BAR size. You should probably disable interrupt
   * Delivery from this device, while querying BAR sizes.
   */
  size_t bar_size(unsigned address)
  {
    uint32_t old = conf_read(address);
    size_t  size = 0;
    
    if ((old & BAR_IO) == 1) {
      // I/O BAR
      conf_write(address, 0xFFFFFFFFU);
      size = ((conf_read(address) & BAR_IO_MASK) ^ 0xFFFFFFFFU) + 1;
      conf_write(address, old);
    } else {
      // Memory BAR
      switch (old & BAR_TYPE_MASK) {
      case BAR_TYPE_32B:
	conf_write(address, 0xFFFFFFFFU);
	size = ((conf_read(address) & BAR_MEM_MASK) ^ 0xFFFFFFFFU) + 1;
	conf_write(address, old);
	break;

      case BAR_TYPE_64B:
	// XXX Support 64-bit BARs.
      case BAR_TYPE_1MB:
      default:
	// Not Supported. Return 0.
	;
      }
    }
    return size;
  }

  /**
   * Searches for a given device.
   */
  unsigned search_device(unsigned theclass, unsigned subclass, unsigned instance, unsigned &irqline, unsigned &irqpin)
  {
    for (unsigned i=0; i<1<<13; i++)
      {
	unsigned char maxfunc = 1;
	for (unsigned func=0; func < maxfunc; func++)
	  {
	    unsigned addr = 0x80000000 | i << 11 | func << 8;
	    if (maxfunc == 1 && conf_read(addr + 0xc) & 0x800000)
	      maxfunc = 8;
	    unsigned value = conf_read(addr + 0x8);
	    if ((theclass == ~0UL || ((value >> 24) & 0xff) == theclass)
		&& (subclass == ~0UL || ((value >> 16) & 0xff) == subclass)
		&& (instance == ~0UL || !instance--))
	      {

		unsigned v = conf_read(addr + 0x3c);
		irqline = v & 0xff00 ? v & 0xff : ~0UL;
		irqpin = (v >> 8) & 0xff;
#if 0
		const char * d = "pciirq:";
		for (char *start = cmdline; start = strstr(start, d); start++)
		  {
		    start += strlen(d);
		    unsigned long bus = strtoul(start, &start, 0);
		    unsigned devfunc = 0;
		    if (start)  devfunc = strtoul(start + 1, &start, 0);
		    start = strstr(start, ",");
		    if (start && (bus << 16 | devfunc << 8) == (addr & ~0x80000000))
		      irqline = strtoul(start + 1, &start, 0);
		  }
#endif
		return addr;
	      }
	  }
      }
    return 0;
  }


  /**
   * Finds a capability for a device.
   */
  unsigned find_cap(unsigned address, unsigned char id)
  {
    if (~conf_read(address+4) & 0x100000) return 0;
    unsigned char cap_offset = conf_read(address + 0x34);
    while (cap_offset)
      {
	if (id == (conf_read(address + cap_offset) & 0xff))
	  return cap_offset;
	else
	  cap_offset = conf_read(address + cap_offset) >> 8;
      }
    return 0;
  }

 HostPci(DBus<MessagePciCfg> bus_pcicfg) : _bus_pcicfg(bus_pcicfg) {};
};
