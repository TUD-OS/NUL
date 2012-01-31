/* -*- Mode: C -*- */

#pragma once

#include <nul/compiler.h>

/* XXX Move to malloc.h */
BEGIN_EXTERN_C
void *realloc(void *ptr, size_t size);
void *malloc(size_t size);
void free(void *ptr);
END_EXTERN_C

/* EOF */
