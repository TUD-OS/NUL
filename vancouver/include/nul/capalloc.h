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
 * allocator and deallocator required which handle concurrency threads well XXX
 */
template <class C>
class CapAllocator {
  public:

  CapAllocator(unsigned long __cap_, unsigned long __cap_start, unsigned long __cap_order) {
    _cap_      = __cap_;
    _cap_start = __cap_start;
    _cap_order = __cap_order;
  }

  static unsigned long _cap_;
  static unsigned long _cap_start;
  static unsigned long _cap_order;

  static inline unsigned alloc_cap(unsigned count = 1) {
    assert(_cap_ < _cap_start + (1 << _cap_order) - 1);
    return Cpu::atomic_xadd(&_cap_, count);
  }
  static inline void dealloc_cap(unsigned cap, unsigned count = 1) {
    while (count--) { unsigned res = nova_revoke(Crd(cap + count, 0, DESC_CAP_ALL), true); assert(res == NOVA_ESUCCESS); }
    // XXX add it back to the cap-allocator
  }
};

template <class C> 
  unsigned long CapAllocator<C>::_cap_;
template <class C> 
  unsigned long CapAllocator<C>::_cap_start;
template <class C> 
  unsigned long CapAllocator<C>::_cap_order;
