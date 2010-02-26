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
 * Features: MemRead, MemWrite
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
  template <class M> bool  receive(M &msg)
  {
    if (!in_range(msg.phys, _base, _size)) return false;
    for (unsigned i=0; i < msg.count; i++)
      reinterpret_cast<char *>(msg.ptr)[i] = 0xff;
    return true;
  }
};


PARAM(nullmem,
      {
	Device *dev = new NullMemDevice(argv[0], argv[1]);
	mb.bus_memread.add(dev, &NullMemDevice::receive_static<MessageMemRead>);
	mb.bus_memwrite.add(dev, &NullMemDevice::receive_static<MessageMemWrite>);
      },
      "nullmem:<range> - ignore Memory access to the given physical address range.",
      "Example: 'nullmem:0xfee00000,0x1000'.");
