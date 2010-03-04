/**
 * Generic PCI classes.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
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

#include "register.h"

enum {
  PCI_CFG_SPACE_DWORDS = 1024,
};


class PciHelper
{
public:
  static unsigned find_free_bdf(DBus<MessagePciConfig> &bus_pcicfg, unsigned bdf)
  {
    if (bdf == ~0ul)
    {
      for (unsigned bus = 0; bus < 256; bus++)
	for (unsigned device = 1; device < 32; device++)
	  {
	    MessagePciConfig msg(0, 0);
	    bdf = (bus << 8) | (device << 3);
	    bus_pcicfg.send(msg, false, bdf);
	    if (msg.value == ~0ul)
	      return bdf;
	  }
    }
    return bdf;
  }
};



/**
 * A template class that provides functions for easier PCI config space implementation.
 */
template <typename Y>
class PciDeviceConfigSpace : public HwRegisterSet< PciDeviceConfigSpace<Y> >
{
 public:
  /**
   * Match a given address to a pci-bar.
   */
  bool match_bar(int index, unsigned long &address)
  {
    unsigned value;
    if (HwRegisterSet< PciDeviceConfigSpace<Y> >::read_reg(index, value))
      {
	unsigned long mask = HwRegisterSet< PciDeviceConfigSpace<Y> >::get_reg_mask(index);
	bool res = !((address ^ value) & mask);
	address &= ~mask;
	return res;
      }
    return false;
  }


  /**
   * The PCI bus transaction function.
   */
  bool receive(MessagePciConfig &msg)
  {
    // config read/write type0 function 0
    if (!msg.bdf)
      {
	bool res;
	if (msg.type == MessagePciConfig::TYPE_READ)
	  {
	    msg.value = 0;
	    res = HwRegisterSet< PciDeviceConfigSpace<Y> >::read_all_regs(msg.dword << 2, msg.value);
	  }
	else
	  res = HwRegisterSet< PciDeviceConfigSpace<Y> >::write_all_regs(msg.dword << 2, msg.value);
	return res;
      }
    return false;
  }
};
