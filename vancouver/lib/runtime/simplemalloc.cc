/**
 * Malloc implementation.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <kauer@tudos.org>
 *
 * This file is part of Florence2.
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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <driver/logging.h>



void *
malloc(unsigned long size)
{
  extern char __mempoolstart, __mempoolend;
  //static int count;
  //Logging::printf("malloc %d - %lx\n", count++, size);
  static char *s =  &__mempoolend;
  s -= size;
  if (s < &__mempoolstart)
    Logging::panic("malloc(%lx) EOM!\n", size);
  return s;
}

void *memalign(unsigned long align, unsigned long size)
{
  // need to be a power of two
  if (align & (align - 1)) return 0;
  malloc((reinterpret_cast<unsigned long>(malloc(0)) - size) & (align-1));
  return malloc(size);
};

void free(void *ptr)
{
  extern char __mempoolstart, __mempoolend;
  assert(ptr <= &__mempoolend && ptr > &__mempoolstart);
  // simplemalloc is simple, so we leak the memory here
}

void *
operator new(unsigned size)
{
  void *ptr = malloc(size);
  if (ptr) memset(ptr, 0, size);
  return ptr;
}

void
operator delete(void *ptr) { free(ptr); }


