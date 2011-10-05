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

/************************************************************************
 * Memory functions.
 ************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

void * memcpy(void *dst, const void *src, unsigned long count);
void * memmove(void *dst, const void *src, unsigned long count);
void * memset(void *dst, int c, unsigned long count); 
int memcmp(const void *dst, const void *src, unsigned long count);

/************************************************************************
 * String functions.
 ************************************************************************/

unsigned long strnlen(const char *src, unsigned long maxlen);
unsigned long strlen(const char *src);

char * strcpy(char *dst, const char *src);
char * strstr(char const *haystack, char const *needle);

unsigned long strtoul(const char *nptr, const char **endptr, int base);
const char * strchr(const char *s, int c);

int strcmp(const char *dst, const char *src);
int strncmp(const char *dst, const char *src, unsigned long size);

int strspn(const char *s, const char *accept);
int strcspn(const char *s, const char *reject);

#ifdef __cplusplus
}
#endif
