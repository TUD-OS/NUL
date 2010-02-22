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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <driver/logging.h>

void *
malloc(size_t size)
{
  extern char __mempoolstart, __mempoolend;
  static char *s =  &__mempoolend;
  s -= size;
  if (s < &__mempoolstart)
    Logging::panic("malloc(%zx) EOM!\n", size);
  return s;
}

void *
realloc(void *ptr, size_t size)
{
  Logging::panic("realloc not implemented\n");
  //return ptr;
}

void *
calloc(size_t n, size_t size)
{
  void *p = malloc(n*size);
  if (p) memset(p, 0, n*size);
  return p;
}

void *memalign(size_t align, size_t size)
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
operator new(unsigned size) { return calloc(1, size); }

void
operator delete(void *ptr) { free(ptr); }

// EOF
