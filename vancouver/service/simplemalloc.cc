/**
 * Malloc implementation.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <bk@vmmon.org>
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
void *memalign_mempool(unsigned long size, unsigned long align)
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



// External interface
void *(*memalign)(unsigned long size, unsigned long align) = memalign_mempool;
void * operator new(unsigned size)   { return memalign(size, 0); }
void   operator delete(void *ptr)    { /* simplemalloc is simple, so we leak the memory here */ }
void * operator new[](unsigned size) { return memalign(size, 0); }
void   operator delete[](void *ptr)  { /* simplemalloc is simple, so we leak the memory here */ }
void * operator new[](unsigned size, unsigned alignment) { return memalign(size, alignment); }
