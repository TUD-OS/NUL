/*
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011, Alexander Boettcher <boettcher@tudos.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once
#include <service/cpu.h>
#include <sys/syscalls.h>

/**
 * Allocates capabilities from a certain range. The range is given by
 * _cap_start and _cap_order parameters.
 *
 * Note: allocator and deallocator required which handle concurrency threads well XXX
 */
class InternalCapAllocator {
  public:
    unsigned alloc_cap(unsigned count = 1); 
    void dealloc_cap(unsigned cap, unsigned count = 1);
};

class CapAllocator : public InternalCapAllocator {
  public:

  unsigned long _cap_;
  unsigned long _cap_start;
  unsigned long _cap_order;

  CapAllocator(unsigned long cap_, unsigned long cap_start, unsigned long cap_order)
    : _cap_(cap_), _cap_start(cap_start), _cap_order(cap_order)
  { }

  unsigned alloc_cap(unsigned count = 1) {
//    assert(_cap_ < _cap_start + (1 << _cap_order) - 1);
//    assert(_cap_ + count < _cap_start + (1 << _cap_order) - 1);
//    assert(_cap_ >= _cap_start);
    return Cpu::atomic_xadd(&_cap_, count);
  }
  void dealloc_cap(unsigned cap, unsigned count = 1) {
//    assert((cap >= _cap_start) && (count <= (1 << _cap_order)));
//    assert( cap + count < _cap_start + (1 << _cap_order) - 1);
    while (count--) { UNUSED unsigned res = nova_revoke(Crd(cap + count, 0, DESC_CAP_ALL), true); assert(res == NOVA_ESUCCESS); }
    // XXX add it back to the cap-allocator
  }
};
