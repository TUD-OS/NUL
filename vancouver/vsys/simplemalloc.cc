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

void *memalign(unsigned long align, unsigned long size)
{
  // needs to be a power of two
  if (align & (align - 1)) return 0;

  extern char __mempoolstart, __mempoolend;
  static char *s =  &__mempoolend;
  s = &__mempoolstart + (((s - &__mempoolstart) - size) & ~(align - 1));
  if (s < &__mempoolstart)
    Logging::panic("malloc(%lx) EOM!\n", size);
  assert(!(reinterpret_cast<unsigned long>(s) & (align - 1)));
  return s;
};


void * malloc(unsigned long size) { return memalign(sizeof(unsigned long), size); }


void free(void *ptr)
{
  extern char __mempoolstart, __mempoolend;
  assert(ptr <= &__mempoolend && ptr > &__mempoolstart);
  // simplemalloc is simple, so we leak the memory here
}


void * operator new(unsigned size) { return malloc(size); }
void   operator delete(void *ptr)  { free(ptr); }
void * operator new[](unsigned size) { return malloc(size); }
void   operator delete[](void *ptr)  { free(ptr); }

// EOF
