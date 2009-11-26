/** -*- Mode: C++ -*-
 * Cpu abstraction with inline asm.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <kauer@tudos.org>
 *
 * This file is part of Vancouver and was developed for Florence2.
 *
 * Florence2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Florence2 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <nova/compiler.h>
#include <nova/types.h>

CPU_BEGIN

// Would be nicer as functions, but not C compatible:
// NOVA_INLINE uint64_t union64(uint32_t high, uint32_t low)
// { unsigned long long res; asm volatile ("" : "=A"(res) : "d"(high), "a"(low)); return res; }
// NOVA_INLINE void split64(uint64_t input, uint32_t &high, uint32_t &low)
// { asm volatile ("" : "=d"(high), "=a"(low) : "A"(input)); }

#define union64(HIGH, LOW)          ({ unsigned long long res; asm volatile ("" : "=A"(res) : "d"(HIGH), "a"(LOW)); res; })
#define split64(INPUT, HIGH, LOW)   asm volatile ("" : "=d"(HIGH), "=a"(LOW) : "A"(INPUT))

NOVA_INLINE void pause() { asm volatile("pause"); }
//NOVA_INLINE void hlt() { asm volatile("hlt"); }

NOVA_INLINE uint32_t cmpxchg(volatile void *var, uint32_t oldvalue, uint32_t newvalue)
{
  asm volatile ("lock; cmpxchg %2, (%0)": "+r"(var), "+a"(oldvalue): "r"(newvalue) : "memory");
  return oldvalue;
}

NOVA_INLINE uint64_t cmpxchg8b(volatile void *var, uint64_t oldvalue, uint64_t newvalue)
{
  uint32_t olow, ohigh; split64(oldvalue, ohigh, olow);
  uint32_t nlow, nhigh; split64(newvalue, nhigh, nlow);
  asm volatile ("lock; cmpxchg8b (%0)": "+r"(var), "+d"(ohigh), "+a"(olow): "c"(nhigh), "b"(nlow) : "memory");
  return union64(ohigh, olow);
}

NOVA_INLINE uint64_t rdtsc()
{
  uint32_t low, high;
  asm volatile ("rdtsc" :  "=a"(low), "=d"(high));
  return union64(high, low);
}

NOVA_INLINE unsigned bsr(uint32_t value)
{
  unsigned res = 0;
  asm volatile ("bsr %1, %0" : "=r"(res) : "r"(value));
  return res;
}

NOVA_INLINE unsigned bsf(uint32_t value)
{
  unsigned res = 0;
  asm volatile ("bsf %1, %0" : "=r"(res) : "r"(value));
  return res;
}

NOVA_INLINE uint32_t cpuid(uint32_t eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
  asm volatile ("cpuid": "+a"(eax), "+b"(*ebx), "+c"(*ecx), "+d"(*edx));
  return eax;
}

NOVA_INLINE unsigned has_vmx() {
  unsigned ebx, ecx, edx;
  cpuid(0x1, &ebx, &ecx, &edx); 
  return ecx & 0x20;
};

NOVA_INLINE unsigned has_svm() {
  unsigned ebx, ecx, edx;
  if (cpuid(0x80000000, &ebx, &ecx, &edx) < 0x8000000A) return false;
  cpuid(0x80000001, &ebx, &ecx, &edx);
  return ecx & 4;
};

#ifdef __cplusplus
template <typename T> NOVA_INLINE void atomic_and(T *ptr, T value) { asm volatile ("lock; and %1, (%0)" :: "r"(ptr), "q"(value)); }
template <typename T> NOVA_INLINE void atomic_or(T *ptr, T value)  { asm volatile ("lock; or %1, (%0)" :: "r"(ptr), "q"(value)); }
#else
// XXX C versions
#endif

NOVA_INLINE long atomic_xadd(long *ptr, long value)
{ asm volatile ("lock; xadd %0, (%1)" : "+r"(value) : "r"(ptr)); return value; }

NOVA_INLINE unsigned minshift(uint32_t start, uint32_t size, unsigned minshift
#ifdef __cplusplus
 = 31
#endif
) {
  unsigned shift = bsf(start | (1ul << (8*sizeof(uint32_t)-1)));
  if (shift < minshift) minshift = shift;
  shift = bsr(size | 1);
  if (shift < minshift) minshift = shift;
  return minshift;
}

CPU_END

// EOF
