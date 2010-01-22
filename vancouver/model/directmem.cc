/**
 * Direct IOIO access.
 *
 * Copyright (C) 2008-2009, Bernhard Kauer <bk@vmmon.org>
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
 * Bridge between guest and host memory.
 *
 * State: unstable
 */
class DirectMemDevice : public StaticReceiver<DirectMemDevice>
{
  const char *debug_getname() { return "DirectMemDevice"; };
  char *_ptr;
  unsigned long _base;
  unsigned long _size;
 public:
  bool  receive(MessageMemMap &msg)
  {
    if (!in_range(msg.phys, _base, _size))  return false;
    msg.phys  = _base;
    msg.ptr   = _ptr;
    msg.count = _size;
    return true;
  }


  DirectMemDevice(char *ptr, unsigned long base, unsigned long size) : _ptr(ptr), _base(base), _size(size) {}
};


PARAM(mio,
      {
	unsigned long size;
	unsigned long dest = (argv[2] == ~0UL) ? argv[0] : argv[2];
	if ( argv[1] == ~0UL)
	  size = 1;
	else
	  size = Cpu::bsr(argv[1] | 1);


	MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, argv[0], 1 << size);
	if (!mb.bus_hostop.send(msg) || !msg.ptr)
	  Logging::panic("can not map IOMEM region %lx+%x", msg.value, msg.len);

	Device *dev = new DirectMemDevice(msg.ptr, dest, 1 << size);
	mb.bus_memmap.add(dev, &DirectMemDevice::receive_static<MessageMemMap>);

      },
      "mio:base,size,dest=base - map hostmemory directly into the VM."
      "Example: 'mio:0xa0000,0x10000'.");
