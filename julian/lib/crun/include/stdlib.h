/* -*- Mode: C -*- */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
# define EXTERN_C extern "C"
#else
# define EXTERN_C
#endif

static inline void abort(void) { *(volatile char *)0xE10 = 0; }

EXTERN_C void *calloc(size_t nmemb, size_t size);
EXTERN_C void *malloc(size_t size);
EXTERN_C void free(void *ptr);
EXTERN_C void *realloc(void *ptr, size_t size);

/* EOF */
