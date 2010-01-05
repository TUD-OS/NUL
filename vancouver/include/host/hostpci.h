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

#include "vmm/motherboard.h"

/**
 * A helper for PCI config space access.
 */
class HostPci
{
  DBus<MessagePciConfig> _bus_pcicfg;

 public:

  unsigned conf_read(unsigned bdf, unsigned short offset)
  {
    MessagePciConfig msg(bdf, offset);
    _bus_pcicfg.send(msg, true);
    return msg.value;
  }

  void conf_write(unsigned bdf, unsigned short offset, unsigned value)
  {
    MessagePciConfig msg(bdf, offset, value);
    _bus_pcicfg.send(msg, true);
  }

  /**
   * Searches for a given device and returns the bdf of it.
   */
  unsigned search_device(unsigned theclass, unsigned subclass, unsigned instance, unsigned &irqline, unsigned &irqpin)
  {
    for (unsigned i=0; i < (1<<13); i++)
      {
	unsigned char maxfunc = 1;
	for (unsigned func=0; func < maxfunc; func++)
	  {
	    unsigned bdf =  i << 3 | func;
	    unsigned value = conf_read(bdf, 0x8);
	    if (value == ~0UL) continue;
	    if (maxfunc == 1 && conf_read(bdf, 0xc) & 0x800000)
	      maxfunc = 8;
	    if ((theclass == ~0UL || ((value >> 24) & 0xff) == theclass)
		&& (subclass == ~0UL || ((value >> 16) & 0xff) == subclass)
		&& (instance == ~0UL || !instance--))
	      {
		unsigned v = conf_read(bdf, 0x3c);
		irqline = v & 0xff00 ? v & 0xff : ~0UL;
		irqpin = (v >> 8) & 0xff;
		return bdf;
	      }
	  }
      }
    return 0;
  }


  /**
   * Finds a capability for a device.
   */
  unsigned find_cap(unsigned bdf, unsigned char id)
  {
    if (~conf_read(bdf, 4) & 0x100000) return 0;
    unsigned char cap_offset = conf_read(bdf, 0x34);
    while (cap_offset)
      {
	if (id == (conf_read(bdf, cap_offset) & 0xff))
	  return cap_offset;
	else
	  cap_offset = conf_read(bdf, cap_offset) >> 8;
      }
    return 0;
  }

 HostPci(DBus<MessagePciConfig> bus_pcicfg) : _bus_pcicfg(bus_pcicfg) {};
};
