/**
 * Standard include file and asm implementation.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <kauer@tudos.org>
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Florence2 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */


#pragma once

#include <stddef.h>

#ifndef __GNUC__
# error Unknown compiler.
#endif

#ifndef __i386
# error Unknown platform.
#endif

#ifdef __cplusplus
# define EXTERN_C extern "C"
#else
# define EXTERN_C
#endif

EXTERN_C void *memcpy(void *dst, const void *src, size_t count);

EXTERN_C void *memmove(void *dst, const void *src, size_t count);
EXTERN_C int memcmp(const void *dst, const void *src, size_t count);
EXTERN_C void *memset(void *dst, int n, size_t count);
EXTERN_C size_t strnlen(const char *src, size_t maxlen);
EXTERN_C size_t strlen(const char *src);
EXTERN_C char *strstr(char *haystack, const char *needle);

/* XXX Belongs in stdlib.h */
EXTERN_C unsigned long strtoul(char *nptr, char **endptr, int base);

EXTERN_C const char *strchr(const char *s, int c);
EXTERN_C char *strsep(char **stringp, const char *delim);
EXTERN_C char *strcpy(char *dst, const char *src);
EXTERN_C char *strncpy(char *dst, const char *src, size_t n);
EXTERN_C int strcmp(const char *dst, const char *src);
EXTERN_C char *strcat(char *dst, const char *src);

/* EOF */
