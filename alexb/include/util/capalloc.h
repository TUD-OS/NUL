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
#include <service/cpu.h>
#include <sys/syscalls.h>

template <unsigned BITS>
class CapAllocatorAtomic {

  protected:
    enum
    {
      BITS_PER_CHAR = 8,
      BITS_PER_UNSIGNED  = sizeof(unsigned)*8,
      BYTES_PER_UNSIGNED = sizeof(unsigned),
    };
  
    // _bits[ceiling(BITS / sizeof(int))]
    unsigned volatile _bits[(BITS + BITS_PER_UNSIGNED - 1) / BITS_PER_UNSIGNED];
    cap_sel _cap_base;

    cap_sel internal_alloc_cap(unsigned count = 1, unsigned byte_start = 0) {
      assert(count == 1);
      assert(_cap_base != ~0UL);

      redo:

      unsigned wrap = 0, i;
      for (i = byte_start; (!wrap || i != byte_start) && _bits[i] == ~0U; wrap += (i + 1) / bytes_max(), i = (i+1) % bytes_max()) {}
      if (wrap && i == byte_start) return 0;

      unsigned _new,_old = _bits[i];

      // _bits may have changed since the for loop. We have to check
      // for the case where all bits are set. Otherwise, we run into
      // undefined behaviour of bsf.
      if (~_old == 0U) goto redo;

      unsigned pos = Cpu::bsf(~_old);
      if (pos > BITS_PER_UNSIGNED) goto redo;
      _new = _old | (1U << pos);
      if (_old != Cpu::cmpxchg4b(&_bits[i], _old, _new)) goto redo;

      //Logging::printf("i=%u 0x%x _old %x, _new %x, pos %u\n", i, i*8*4, _old, _new, pos);
      return _cap_base + i * BITS_PER_UNSIGNED + pos;
    }

  public:

    CapAllocatorAtomic(cap_sel _cap_start = ~0UL) //~0U means disabled
      : _bits(), _cap_base(_cap_start)
    { }

    /*
     * Get maximum number of bits represented by this field.
     */
    unsigned idx_max()   const { return sizeof(_bits) * BITS_PER_CHAR; }
    unsigned bytes_max() const { return sizeof(_bits) / BYTES_PER_UNSIGNED; }
    cap_sel  idx_smallest() { return _cap_base; }

    cap_sel alloc_cap(unsigned count = 1) { return internal_alloc_cap(count); }

    void dealloc_cap(cap_sel cap, unsigned count = 1) {
      assert(_cap_base != ~0UL);
      assert (cap >= _cap_base && (cap - _cap_base + (count ? count - 1 : 0)) < idx_max());
      while (count--) {
        unsigned i   = (cap + count - _cap_base) / BITS_PER_UNSIGNED;
        unsigned pos = (cap + count - _cap_base) % BITS_PER_UNSIGNED;

        unsigned res = nova_revoke(Crd(cap + count, 0, DESC_CAP_ALL), true);
        assert(res == NOVA_ESUCCESS);

        redo:

        unsigned _new, _old = _bits[i];
        if (!(_old & (1U << pos))) {
          Logging::panic("cap index already freed\n");
          return;
        }

        _new = _old & ~(1U << pos);
        unsigned _got = Cpu::cmpxchg4b(&_bits[i], _old, _new);
        if (_got != _old) goto redo;
      }
    }
};

