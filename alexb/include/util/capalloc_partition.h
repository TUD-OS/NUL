/*
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

#include <nul/baseprogram.h>
#include "capalloc.h"

template <unsigned BITS, bool error_doublefree = true>
class CapAllocatorAtomicPartition : public CapAllocatorAtomic<BITS, error_doublefree> {

protected:
    unsigned _divider;

public:
  CapAllocatorAtomicPartition(unsigned long _cap_start = ~0UL, unsigned divider = 1) //~0UL means disabled
     : CapAllocatorAtomic<BITS, error_doublefree>(_cap_start), _divider(divider) {}

  unsigned alloc_cap(unsigned count = 1, unsigned cpu = ~0U) {
    if (cpu == ~0U) cpu = BaseProgram::mycpu();
    unsigned start = (cpu * (BITS / _divider / CapAllocatorAtomic<BITS, error_doublefree>::BITS_PER_INT));
    start %= CapAllocatorAtomic<BITS, error_doublefree>::bytes_max();
    unsigned res = CapAllocatorAtomic<BITS, error_doublefree>::internal_alloc_cap(count, start);

//    Logging::printf("cap=%x cpu %u/%u (valid range %lx %lx)\n", res, cpu, _divider,
//                    CapAllocatorAtomic<BITS, error_doublefree>::_cap_base,
//                    CapAllocatorAtomic<BITS, error_doublefree>::_cap_base
//                     + CapAllocatorAtomic<BITS, error_doublefree>::idx_max());
    return res;
  }
};
