/**
 * PCI config space access.
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

/**
 * Provide HW PCI config space access by bridging PCI cfg read/write
 * messages to the HW IO busses.
 */
class PciConfigAccess : public StaticReceiver<PciConfigAccess>
{
  static const unsigned BASE = 0xcf8;
  DBus<MessageIOIn>  _hwioin;
  DBus<MessageIOOut> _hwioout;
  const char *debug_getname() { return "PCIAccess"; };
public:
  
  PciConfigAccess(DBus<MessageIOIn> hwioin, DBus<MessageIOOut> hwioout) : _hwioin(hwioin), _hwioout(hwioout) {};
  bool  receive(MessagePciCfg &msg)
  {
    MessageIOOut msg1(MessageIOOut::TYPE_OUTL, BASE, msg.address);
    if (_hwioout.send(msg1))
      {
	if (msg.type == MessagePciCfg::TYPE_READ)
	  {
	    MessageIOIn msg2(MessageIOIn::TYPE_INL, BASE+4);
	    bool res = _hwioin.send(msg2);
	    msg.value = msg2.value;
	    return res;
	  }
	else
	  {
	    MessageIOOut msg2(MessageIOOut::TYPE_OUTL, BASE+4, msg.value);
	    return _hwioout.send(msg2);
	  }
      }
    return false;
  }
};

PARAM(pcicfg,
      {
	Device *dev = new PciConfigAccess(mb.bus_hwioin, mb.bus_hwioout);
	mb.bus_hwpcicfg.add(dev, &PciConfigAccess::receive_static<MessagePciCfg>);
      },
      "pcicfg - provide HW PCI config space access.");
