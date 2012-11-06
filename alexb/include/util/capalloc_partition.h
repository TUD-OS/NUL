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

template <unsigned BITS>
class CapAllocatorAtomicPartition : public CapAllocatorAtomic<BITS> {

protected:
    unsigned _divider;

public:
  CapAllocatorAtomicPartition(cap_sel _cap_start = ~0UL, unsigned divider = 1) //~0UL means disabled
     : CapAllocatorAtomic<BITS>(_cap_start), _divider(divider) {}

  cap_sel alloc_cap(unsigned count = 1, unsigned cpu = ~0U) {
    if (cpu == ~0U) cpu = BaseProgram::mycpu();
    unsigned start = (cpu * (BITS / _divider / CapAllocatorAtomic<BITS>::BITS_PER_UNSIGNED));
    start %= CapAllocatorAtomic<BITS>::bytes_max();
    unsigned res = CapAllocatorAtomic<BITS>::internal_alloc_cap(count, start);
/*
    Logging::printf("cap=%x cpu %u/%u (valid range %x %x, cpu starts at %x)\n", res, cpu, _divider,
                    CapAllocatorAtomic<BITS>::_cap_base,
                    CapAllocatorAtomic<BITS>::_cap_base
                     + CapAllocatorAtomic<BITS>::bytes_max(),
                    CapAllocatorAtomic<BITS>::_cap_base + start);
*/
    return res;
  }
};
