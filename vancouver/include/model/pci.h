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
 * Template that forwards PCI config messages to the corresponding
 * register functions.
 */
template <typename Y> class PciConfigHelper {
public:
  bool receive(MessagePciConfig &msg) {
    // config read/write type0 function 0
    if (!msg.bdf) {
      if (msg.type == MessagePciConfig::TYPE_WRITE)
	return reinterpret_cast<Y *>(this)->PCI_write(msg.dword, msg.value);
      msg.value = 0;
      return reinterpret_cast<Y *>(this)->PCI_read(msg.dword, msg.value);
    }
    return false;
  };
};
