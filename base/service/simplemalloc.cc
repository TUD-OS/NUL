/** @file
 * Malloc implementation.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <bk@vmmon.org>
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

#include "service/helper.h"
#include "service/string.h"
#include "service/logging.h"
#include "service/cpu.h"

/**
 * Alloc memory from a mempool defined in the linker script.
 */
void *memalloc_mempool(unsigned long size, unsigned long align)
{
  // align needs to be a power of two
  assert(!(align & (align - 1)));
  // Force a minimum alignment of 16 to be on the safe side with SSE
  // and atomic ops.
  if (align < 16) align = 16;

  extern char __mempoolstart, __mempoolend;
  static char *s = &__mempoolend;
  unsigned old, _new;
  do {
    old = reinterpret_cast<unsigned>(s);
    _new = static_cast<unsigned>((old - size) & ~(align - 1));
  } while (old != Cpu::cmpxchg4b(reinterpret_cast<unsigned*>(&s), old, _new));

  if (reinterpret_cast<char*>(_new) < &__mempoolstart)  Logging::panic("%s(size=%#lx heap region=%p-%p) EOM - increase __memsize!\n", __func__, size, &__mempoolstart, &__mempoolend);
  assert(!(_new & (align - 1)));
  return reinterpret_cast<char*>(_new);
}


void memfree_mempool(void *) { /* simplemalloc is simple, so we leak the memory here */ }



// External interface
void *(*memalloc)(unsigned long size, unsigned long align) = memalloc_mempool;
void * operator new(unsigned size)   { return memalloc(size, 0); }
void * operator new[](unsigned size) { return memalloc(size, 0); }
void * operator new[](unsigned size, unsigned alignment) { return memalloc(size, alignment); }
void * operator new(unsigned size, unsigned alignment) { return memalloc(size, alignment); }
void (*memfree)(void *ptr) = memfree_mempool;
void   operator delete(void *ptr)    { memfree(ptr); }
void   operator delete[](void *ptr)  { memfree(ptr);  }
