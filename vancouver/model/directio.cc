/** @file
 * Direct IOIO access.
 *
 * Copyright (C) 2008-2009, Bernhard Kauer <bk@vmmon.org>
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

#include "nul/motherboard.h"


/**
 * Bridge between guest and host IOIO busses.
 *
 * State: stable
 * Features: IOIn, IOOut
 */
class DirectIODevice : public StaticReceiver<DirectIODevice>
{
  DBus<MessageHwIOIn>  &_bus_hwioin;
  DBus<MessageHwIOOut> &_bus_hwioout;
  unsigned _base;
  unsigned _size;

 public:
  bool  receive(MessageIOIn &msg)  {  if (in_range(msg.port, _base, _size)) return _bus_hwioin.send(static_cast<MessageHwIOIn&>(msg), true);  return false; }
  bool  receive(MessageIOOut &msg) {  if (in_range(msg.port, _base, _size)) return _bus_hwioout.send(static_cast<MessageHwIOOut&>(msg), true); return false; }
  DirectIODevice(DBus<MessageHwIOIn> &bus_hwioin, DBus<MessageHwIOOut> &bus_hwioout, unsigned base, unsigned size)
  : _bus_hwioin(bus_hwioin), _bus_hwioout(bus_hwioout), _base(base), _size(size) {}
};


PARAM_HANDLER(dio,
	      "dio:<range> - directly assign ioports to the VM.",
	      "Example: 'dio:0x3f8+8'.",
	      "Forward access to given ioports to the hardware ones.",
	      "Please note that a 'ioio' as backend for this device is needed too.")
{
  unsigned short base = argv[0];
  unsigned short order;
  if ( argv[1] == ~0UL)
    order = 1;
  else
    order = Cpu::bsr(argv[1] | 1);

  // request the io ports
  MessageHostOp msg(MessageHostOp::OP_ALLOC_IOIO_REGION, (base << 8) |  order);
  if (!mb.bus_hostop.send(msg))
    Logging::panic("%s: failed to allocate ports %x/%u\n", __PRETTY_FUNCTION__, base, order);

  DirectIODevice *dev = new DirectIODevice(mb.bus_hwioin, mb.bus_hwioout, base, 1 << order);
  mb.bus_ioin.add(dev,  DirectIODevice::receive_static<MessageIOIn>);
  mb.bus_ioout.add(dev, DirectIODevice::receive_static<MessageIOOut>);
}
