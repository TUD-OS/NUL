/** @file
 * Standard include file and asm implementation.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <kauer@tudos.org>
 * Copyright (C) 2011, Alexander Boettcher <boettcher@tudos.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <nul/types.h>

/************************************************************************
 * Memory functions.
 ************************************************************************/

BEGIN_EXTERN_C

void * memcpy(void *dst, const void *src, size_t count);
void * memmove(void *dst, const void *src, size_t count);
void * memset(void *dst, int c, size_t count); 
int memcmp(const void *dst, const void *src, size_t count);

/************************************************************************
 * String functions.
 ************************************************************************/

size_t strnlen(const char *src, size_t maxlen);
size_t strlen(const char *src);

char * strcpy(char *dst, const char *src);
char * strstr(char const *haystack, char const *needle);

unsigned long strtoul(const char *nptr, const char **endptr, int base);
const char * strchr(const char *s, int c);

int strcmp(const char *dst, const char *src);
int strncmp(const char *dst, const char *src, size_t size);

int strspn(const char *s, const char *accept);
int strcspn(const char *s, const char *reject);

END_EXTERN_C
