/** @file
 * Generic PCI classes.
 *
 * Copyright (C) 2008-2010, Bernhard Kauer <bk@vmmon.org>
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
	    bdf = (bus << 8) | (device << 3);
	    MessagePciConfig msg(bdf, 0);
	    bus_pcicfg.send(msg, false);
	    if (msg.value == ~0ul) return bdf;
	  }
    }
    return bdf;
  }

  /**
   * Template that forwards PCI config messages to the corresponding
   * register functions.
   */
  template <typename Y> static bool receive(MessagePciConfig &msg, Y *obj, unsigned bdf) {

    if (msg.bdf != bdf)    return false;

    if (msg.type == MessagePciConfig::TYPE_WRITE)  return obj->PCI_write(msg.dword, msg.value);
    msg.value = 0;
    return obj->PCI_read(msg.dword, msg.value);
  };

};
