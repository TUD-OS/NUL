/** @file
 * Null Memory access.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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
 */
class NullMemDevice : public StaticReceiver<NullMemDevice>
{
  unsigned long _base;
  unsigned long _size;

public:
  NullMemDevice(unsigned long base, unsigned long size) : _base(base), _size(size) {}
  bool  receive(MessageMem &msg)
  {
    if (!in_range(msg.phys, _base, _size)) return false;
    if (msg.read) *msg.ptr = 0xffffffff;
    return true;
  }
};


PARAM_HANDLER(nullmem,
      "nullmem:<range> - ignore Memory access to the given physical address range.",
      "Example: 'nullmem:0xfee00000,0x1000'.")
{
  mb.bus_mem.add(new NullMemDevice(argv[0], argv[1]), NullMemDevice::receive_static<MessageMem>);
}

