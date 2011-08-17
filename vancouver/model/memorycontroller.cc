/** @file
 * Physical Memory handling.
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

class MemoryController : public StaticReceiver<MemoryController>
{
  char *_physmem;
  unsigned long _start;
  unsigned long _end;


public:
  /****************************************************/
  /* Physmem access                                   */
  /****************************************************/
  bool  receive(MessageMem &msg)
  {
    if ((msg.phys < _start) || (msg.phys >= (_end - 4)))  return false;
    unsigned *ptr = reinterpret_cast<unsigned *>(_physmem + msg.phys);

    if (msg.read) *msg.ptr = *ptr; else *ptr = *msg.ptr;
    return true;
  }


  bool  receive(MessageMemRegion &msg)
  {
    if ((msg.page < (_start >> 12)) || (msg.page >= (_end >> 12)))  return false;
    msg.start_page = _start >> 12;
    msg.count = (_end - _start) >> 12;
    msg.ptr = _physmem + _start;
    return true;
  }


  MemoryController(char *physmem, unsigned long start, unsigned long end) : _physmem(physmem), _start(start), _end(end) {}
};


PARAM_HANDLER(mem,
		      "mem:start=0:end=~0 - create a memory controller that handles physical memory accesses.",
		      "Example: 'mem:0,0xa0000' for the first 640k region",
		      "Example: 'mem:0x100000' for all the memory above 1M")
{

  MessageHostOp msg(MessageHostOp::OP_GUEST_MEM, 0UL);
  if (!mb.bus_hostop.send(msg))
    Logging::panic("%s failed to get physical memory\n", __PRETTY_FUNCTION__);
  unsigned long start = ~argv[0] ? argv[0] : 0;
  unsigned long end   = argv[1] > msg.len ? msg.len : argv[1];
  Logging::printf("physmem: %lx [%lx, %lx]\n", msg.value, start, end);
  MemoryController *dev = new MemoryController(msg.ptr, start, end);
  // physmem access
  mb.bus_mem.add(dev,       MemoryController::receive_static<MessageMem>);
  mb.bus_memregion.add(dev, MemoryController::receive_static<MessageMemRegion>);
}
