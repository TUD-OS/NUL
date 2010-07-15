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

#include "nul/motherboard.h"

/**
 * Provide HW PCI config space access by bridging PCI cfg read/write
 * messages to the HW IO busses.
 *
 * State: stable
 * Documentation: pci3 spec
 */
struct PciConfigAccess : public StaticReceiver<PciConfigAccess>
{
  static const unsigned BASE = 0xcf8;
  DBus<MessageIOIn>  &_hwioin;
  DBus<MessageIOOut> &_hwioout;

  PciConfigAccess(DBus<MessageIOIn> &hwioin, DBus<MessageIOOut> &hwioout) : _hwioin(hwioin), _hwioout(hwioout) {};
  bool  receive(MessagePciConfig &msg) {

    if (msg.dword >= 0x40 || (msg.bdf >= 0x10000)) return false;

    MessageIOOut msg1(MessageIOOut::TYPE_OUTL, BASE, 0x80000000 |  (msg.bdf << 8) | (msg.dword << 2));
    if (!_hwioout.send(msg1, true)) return false;

    MessageIOOut msg2(MessageIOOut::TYPE_OUTL, BASE+4, msg.value);
    if (msg.type == MessagePciConfig::TYPE_WRITE) return _hwioout.send(msg2, true);

    MessageIOIn msg3(MessageIOIn::TYPE_INL, BASE+4);
    bool res = _hwioin.send(msg3, true);
    msg.value = msg3.value;
    return res;
  }
};


PARAM(pcicfg,
      {
	MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION,  (PciConfigAccess::BASE << 8) | 3);
	check0(!mb.bus_hostop.send(msg1), "%s could not allocate ioports %x+8\n", __PRETTY_FUNCTION__, PciConfigAccess::BASE);

	Device *dev = new PciConfigAccess(mb.bus_hwioin, mb.bus_hwioout);
	mb.bus_hwpcicfg.add(dev, PciConfigAccess::receive_static<MessagePciConfig>);
      },
      "pcicfg - provide HW PCI config space access through IO ports 0xcf8/0xcfc.");
