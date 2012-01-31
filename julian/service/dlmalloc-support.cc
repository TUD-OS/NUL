// -*- Mode: C++ -*-

#include <nul/compiler.h>
#include <nul/types.h>
#include <service/helper.h>
#include <service/logging.h>
#include <sys/syscalls.h>

#include "dlmalloc-config.h"

EXTERN_C void* dlmemalign(size_t, size_t);
EXTERN_C void  dlfree(void*);

// Make this usable for C code.
EXTERN_C void dlmalloc_init(cap_sel pool);
EXTERN_C void dlmalloc_init_locks(void);

// POSIX glue

void abort()
{
  Logging::panic("Abort!");
}

// Semaphore glue

static cap_sel cap_pool;

void semaphore_init(cap_sel *lk, unsigned initial)
{
  *lk = __sync_fetch_and_add(&cap_pool, 1);
  int res = nova_create_sm(*lk, initial);
  assert(res == NOVA_ESUCCESS);
}

void semaphore_down(cap_sel *lk)
{
  int res = nova_semdown(*lk);
  assert(res == NOVA_ESUCCESS);
}

void semaphore_up(cap_sel *lk)
{
  int res = nova_semup(*lk);
  assert(res == NOVA_ESUCCESS);
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

void dlmalloc_init(cap_sel dlmalloc_cap_pool)
{
  cap_pool = dlmalloc_cap_pool;
  dlmalloc_init_locks();
  memalloc = dlmalloc_memalloc;
  memfree  = dlfree;
}

// EOF
