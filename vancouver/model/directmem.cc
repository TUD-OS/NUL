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
  unsigned long _phys;
  unsigned long _size;
 public:
  bool  receive(MessageMemMap &msg)
  {
    if (!in_range(msg.phys, _phys, _size))  return false;
    Logging::printf("%s: %p base %lx+%lx\n", __PRETTY_FUNCTION__, _ptr, _phys, _size);
    msg.phys  = _phys;
    msg.ptr   = _ptr;
    msg.count = _size;
    return true;
  }


  bool  receive(MessageMemRead &msg)
  {
    if (in_range(msg.phys, _phys, _size - msg.count))
      memcpy(msg.ptr, _ptr + msg.phys - _phys, msg.count);
    else return false;

    return true;
  }


  bool  receive(MessageMemWrite &msg)
  {
    if (in_range(msg.phys, _phys, _size - msg.count))
      memcpy(_ptr + msg.phys - _phys, msg.ptr, msg.count);
    else return false;

    return true;
  }
  DirectMemDevice(char *ptr, unsigned long phys, unsigned long size) : _ptr(ptr), _phys(phys), _size(size)
  {
    Logging::printf("DirectMem: %p base %lx+%lx\n", ptr, phys, size);
  }
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
	mb.bus_memmap.add(dev,  &DirectMemDevice::receive_static<MessageMemMap>);
	mb.bus_memread.add(dev, &DirectMemDevice::receive_static<MessageMemRead>);
	mb.bus_memwrite.add(dev,&DirectMemDevice::receive_static<MessageMemWrite>);

      },
      "mio:base,size,dest=base - map hostmemory directly into the VM."
      "Example: 'mio:0xa0000,0x10000'.");
