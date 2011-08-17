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
 * Bridge between guest and host memory.
 *
 * State: testing
 */
class DirectMemDevice : public StaticReceiver<DirectMemDevice>
{
  char *_ptr;
  unsigned long _phys;
  unsigned long _size;
 public:
  bool  receive(MessageMemRegion &msg)
  {
    if (!in_range(msg.page, _phys >> 12, _size >> 12))  return false;
    Logging::printf("%s: %p base %lx+%lx\n", __PRETTY_FUNCTION__, _ptr, _phys, _size);
    msg.start_page = _phys >> 12;
    msg.count = _size >> 12;
    msg.ptr   = _ptr;
    return true;
  }


  bool  receive(MessageMem &msg)
  {
    unsigned *ptr;
    if (in_range(msg.phys, _phys, _size))
      ptr = reinterpret_cast<unsigned *>(_ptr + msg.phys - _phys);
    else return false;

    if (msg.read) *msg.ptr = *ptr; else *ptr = *msg.ptr;
    return true;
  }


  DirectMemDevice(char *ptr, unsigned long phys, unsigned long size) : _ptr(ptr), _phys(phys), _size(size)
  {
    Logging::printf("DirectMem: %p base %lx+%lx\n", ptr, phys, size);
  }
};


PARAM_HANDLER(mio,
	      "mio:base,size,dest=base - map hostmemory directly into the VM.",
	      "Example: 'mio:0xa0000,0x10000'.")
{
  unsigned long size;
  unsigned long dest = (argv[2] == ~0UL) ? argv[0] : argv[2];
  if ( argv[1] == ~0UL)
    size = 1;
  else
    size = Cpu::bsr(argv[1] | 1);


  MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, argv[0], 1 << size);
  if (!mb.bus_hostop.send(msg) || !msg.ptr)
    Logging::panic("can not map IOMEM region %lx+%lx", msg.value, msg.len);

  DirectMemDevice *dev = new DirectMemDevice(msg.ptr, dest, 1 << size);
  mb.bus_memregion.add(dev,  DirectMemDevice::receive_static<MessageMemRegion>);
  mb.bus_mem.add(dev,        DirectMemDevice::receive_static<MessageMem>);

}

