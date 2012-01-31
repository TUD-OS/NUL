// -*- Mode: C++ -*-

#include <nul/compiler.h>
#include <nul/types.h>
#include <service/helper.h>
#include <service/logging.h>

#include "dlmalloc-config.h"

EXTERN_C void* dlmemalign(size_t, size_t);
EXTERN_C void  dlfree(void*);

// Make this usable for C code.
EXTERN_C void dlmalloc_init();
EXTERN_C void dlmalloc_init_locks(void);

// POSIX glue

void abort()
{
  Logging::panic("Abort!");
}

// Semaphore glue

void semaphore_init(cap_sel *lk, int initial)
{

}

void semaphore_down(cap_sel *lk)
{

}

void semaphore_up(cap_sel *lk)
{

}

void *sbrk(size_t size)
{
  return memalloc_mempool(size, 4096);
}

// External interface

static void *dlmalloc_memalloc(unsigned long size, unsigned long align)
{
  return dlmemalign(align, size);
}

void dlmalloc_init()
{
  dlmalloc_init_locks();
  memalloc = dlmalloc_memalloc;
  memfree  = dlfree;
}

// EOF
