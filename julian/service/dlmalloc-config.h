/* -*- Mode: C -*- */

#pragma once

/* Configuration flags for dlmalloc */

/* #if (DLMALLOC_VERSION != 20805) */
/* # error Wrong dlmalloc version. If you updated it, please check this file as well. */
/* #endif */

#define USE_DL_PREFIX           /* Use dl prefix for all exports */

#define MALLOC_FAILURE_ACTION   /* empty */
#define USE_LOCKS               2
#define HAVE_MORECORE           1
#define HAVE_MMAP               0
#define HAVE_MREMAP             0

#define LACKS_UNISTD_H
#define LACKS_FCNTL_H
#define LACKS_SYS_PARAM_H
#define LACKS_SYS_MMAN_H
#define LACKS_SYS_TYPES_H
#define LACKS_ERRNO_H
#define LACKS_SCHED_H
#define LACKS_TIME_H
#define DEFAULT_GRANULARITY     (128 * 1024)      /* 128K */
#define MALLOC_ALIGNMENT        16                /* important for SSE */

/* C compatibility */

#include <nul/compiler.h>
#include <nul/types.h>
#include <service/string.h>

EXTERN_C int printf(const char *msg, ...);
#define fprintf(f, ...) printf(__VA_ARGS__)
EXTERN_C void abort() NORETURN;

EXTERN_C void *sbrk(size_t size);

#ifndef assert
# define assert(x) do {if (!(x)) abort(); } while (0) /* XXX */
#endif

EXTERN_C void semaphore_init(cap_sel *lk, int initial);
EXTERN_C void semaphore_destroy(cap_sel *lk);
EXTERN_C void semaphore_down(cap_sel *lk);
EXTERN_C void semaphore_up(cap_sel *lk);

#define EINVAL -1
#define ENOMEM -1


/* EOF */
