/**
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


/**
 * Alloc memory from a mempool defined in the linker script.
 */
void *memalloc_mempool(unsigned long size, unsigned long align)
{
  // align needs to be a power of two
  assert(!(align & (align - 1)));
  if (align < sizeof(unsigned long)) align = sizeof(unsigned long);

  extern char __mempoolstart, __mempoolend;
  static char *s = &__mempoolend;
  s = reinterpret_cast<char *>((reinterpret_cast<unsigned long>(s) - size) & ~(align - 1));
  if (s < &__mempoolstart)  Logging::panic("%s(%lx) EOM - increase __memsize!\n", __func__, size);
  assert(!(reinterpret_cast<unsigned long>(s) & (align - 1)));
  return s;
}


void memfree_mempool(void *) { /* simplemalloc is simple, so we leak the memory here */ }



// External interface
void *(*memalloc)(unsigned long size, unsigned long align) = memalloc_mempool;
void * operator new(unsigned size)   { return memalloc(size, 0); }
void * operator new[](unsigned size) { return memalloc(size, 0); }
void * operator new[](unsigned size, unsigned alignment) { return memalloc(size, alignment); }
void (*memfree)(void *ptr) = memfree_mempool;
void   operator delete(void *ptr)    { memfree(ptr); }
void   operator delete[](void *ptr)  { memfree(ptr);  }
