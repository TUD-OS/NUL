/** @file
 * Null IOIO access.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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
 * Device that ignores all operations.
 *
 * State: stable
 * Features: IOIn, IOOut
 */
class NullIODevice : public StaticReceiver<NullIODevice>
{
  unsigned _base;
  unsigned _size;
  unsigned _value;

 public:
  NullIODevice(unsigned base, unsigned size, unsigned value) : _base(base), _size(size), _value(value) {}
  bool  receive(MessageIOOut &msg) { return in_range(msg.port, _base, _size); }
  bool  receive(MessageIOIn  &msg) {
    if (!in_range(msg.port, _base, _size)) return false;
    if (_value != ~0U)  msg.value = _value;
    return true;
  }
};


PARAM_HANDLER(nullio,
	      "nullio:<range>[,value] - ignore IOIO at given port range. An optional value can be given to return a fixed value on read..",
	      "Example: 'nullio:0x80+1'.")
{
  NullIODevice *dev = new NullIODevice(argv[0], argv[1] == ~0UL ? 1 : argv[1], argv[2]);
  mb.bus_ioin.add(dev,  NullIODevice::receive_static<MessageIOIn>);
  mb.bus_ioout.add(dev, NullIODevice::receive_static<MessageIOOut>);
}

