/**
 * Null Memory access.
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
 * Device that ignores all operations.
 *
 * State: stable
 */
class NullMemDevice : public StaticReceiver<NullMemDevice>
{
  unsigned long _base;
  unsigned long _size;

  const char *debug_getname() { return "NullMemDevice"; };
  void debug_dump() {
    Device::debug_dump();
    Logging::printf("   %4lx+%lx", _base, _size);
  };
public:
  NullMemDevice(unsigned long base, unsigned long size) : _base(base), _size(size) {}
  bool  receive(MessageMem &msg)
  {
    if (!in_range(msg.phys, _base, _size)) return false;
    if (msg.read) *msg.ptr = 0xffffffff;
    return true;
  }
};


PARAM(nullmem,
      {
	Device *dev = new NullMemDevice(argv[0], argv[1]);
	mb.bus_mem.add(dev, &NullMemDevice::receive_static<MessageMem>);
      },
      "nullmem:<range> - ignore Memory access to the given physical address range.",
      "Example: 'nullmem:0xfee00000,0x1000'.");
