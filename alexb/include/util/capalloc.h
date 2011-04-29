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

template <unsigned BITS, bool error_doublefree = true>
class CapAllocatorAtomic {

  protected:
    enum
    {
      BITS_PER_CHAR = 8,
      BITS_PER_INT = 32,
      BYTES_PER_INT = 4,
    };
    union {
      unsigned volatile a[(BITS + BITS_PER_INT - 1) / BITS_PER_INT];
      void * p;
    } _bits;
    unsigned long _cap_base;

    /*
     * Get maximum number of bits represented by this field.
     */
    inline unsigned idx_max() const { return sizeof(_bits) * BITS_PER_CHAR; }
    inline unsigned bytes_max() const { return sizeof(_bits) / BYTES_PER_INT; }

    unsigned __alloc_cap(unsigned count = 1, unsigned byte_start = 0) {
      assert(count == 1);
      assert(_cap_base != ~0UL);

      redo:

      unsigned wrap = 0, i;
      for (i = byte_start; (!wrap || i != byte_start) && _bits.a[i] == ~0U; wrap += (i + 1) / bytes_max(), i = (i+1) % bytes_max());
      if (wrap && i == byte_start) return 0;

      unsigned _new,_old = _bits.a[i];
      unsigned pos = Cpu::bsf(~_old);
      if (pos > BITS_PER_INT) goto redo;
      _new = _old | (1U << pos);
      if (_old != Cpu::cmpxchg4b(&_bits.a[i], _old, _new)) goto redo;

      //Logging::printf("i=%u 0x%x _old %x, _new %x, pos %u\n", i, i*8*4, _old, _new, pos);
      return _cap_base + i * BITS_PER_INT + pos;
    }

  public:

    CapAllocatorAtomic(unsigned long __cap_start = ~0UL) { //~0U means disabled
      _bits.p = reinterpret_cast<void *>(reinterpret_cast<unsigned long>(_bits.a));
      memset(_bits.p, 0U, sizeof(_bits.a));
      _cap_base = __cap_start;
    }

    unsigned alloc_cap(unsigned count = 1) { return __alloc_cap(count); }

    void dealloc_cap(unsigned cap, unsigned count = 1) {
      assert(_cap_base != ~0UL);
      assert (cap >= _cap_base && (cap - _cap_base + (count ? count - 1 : 0)) < idx_max());
      while (count--) {
        unsigned i   = (cap + count - _cap_base) / BITS_PER_INT;
        unsigned pos = (cap + count - _cap_base) % BITS_PER_INT;

        redo:

        unsigned _new, _old = _bits.a[i];
        if (!(_old & (1U << pos))) {
          if (error_doublefree) Logging::panic("double free detected\n");
          //someone else freed the slot and it is ok according to error_doublefree variable
          return;
        }

        _new = _old & ~(1U << pos);
        unsigned _got = Cpu::cmpxchg4b(&_bits.a[i], _old, _new);
        if (_got != _old) goto redo;

        unsigned res = nova_revoke(Crd(cap + count, 0, DESC_CAP_ALL), true);
        assert(res == NOVA_ESUCCESS);
      }
    }
};

