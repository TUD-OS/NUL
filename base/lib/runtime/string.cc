/** @file
 * Standard include file and asm implementation.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <kauer@tudos.org>
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

#include <service/string.h>

/************************************************************************
 * Memory functions.
 ************************************************************************/

BEGIN_EXTERN_C

void *memcpy(void *dst, const void *src, size_t count) {

  void *res = dst;
  if (count & 1) asm volatile ("movsb" : "+D"(dst), "+S"(src) : : "memory");
  if (count & 2) asm volatile ("movsw" : "+D"(dst), "+S"(src) : : "memory");
  count /= 4;
  asm volatile ("rep movsl" : "+D"(dst), "+S"(src), "+c" (count) : : "memory");
  return res;
}


void * memmove(void *dst, const void *src, size_t count) {

  char *d = reinterpret_cast<char *>(dst);
  const char *s = reinterpret_cast<const char *>(src);
  if (d <= s || d >= (s+count)) return memcpy(d, s, count);

  d += count-1;
  s += count-1;
  asm volatile ("std; rep movsb; cld;" : "+D"(d), "+S"(s), "+c"(count) : : "memory");
  return dst;
}


void * memset(void *dst, int c, size_t count) {

  void *res = dst;
  unsigned value = (c & 0xff) * 0x01010101;
  if (count & 1) asm volatile ("stosb" : "+D"(dst) : "a"(value) : "memory");
  if (count & 2) asm volatile ("stosw" : "+D"(dst) : "a"(value) : "memory");
  count /= 4;
  asm volatile ("rep stosl" : "+D"(dst), "+c"(count) : "a"(value)  : "memory");
  return res;
}


int memcmp(const void *dst, const void *src, size_t count) {
  const char *d = reinterpret_cast<const char *>(dst);
  const char *s = reinterpret_cast<const char *>(src);
  unsigned diff = 0;
  while (!diff && count--) diff = *d++ - *s++;
  return diff;
}




/************************************************************************
 * String functions.
 ************************************************************************/


size_t strnlen(const char *src, size_t maxlen) {
  size_t i=0;
  while (src[i] && maxlen--) i++;
  return i;
}


size_t strlen(const char *src) { return strnlen(src, ~0ul); }


char * strcpy(char *dst, const char *src)  {
  char *res = dst;
  unsigned char ch;
  do {
    ch = *src++;
    *dst++ = ch;
  } while (ch);
  return res;
}


char * strstr(char const *haystack, char const *needle) {
  int index;
  while (*haystack) {
    for (index=0; needle[index] == haystack[index] && needle[index];)
       index++;
    if (!needle[index]) return const_cast<char *>(haystack);
    haystack += index ? index : 1;
  }
  return 0;
}


unsigned long strtoul(const char *nptr, const char **endptr, int base) {

  unsigned long res = 0;
  if ((!base || base == 16) && *nptr == '0' && *(nptr+1) == 'x') {
    nptr += 2;
    base = 16;
  }
  else if (!base)
    base = (*nptr == '0') ? 8 : 10;

  while (*nptr) {
    long val = *nptr - '0';
    // a lower or upper character
    if (val > 9)
      val = (*nptr | 0x20) - 'a' + 10;
    if (val < 0 || val >= base)
      break;
    res = res*base + val;
    nptr++;
  }
  if (endptr) *endptr = nptr;
  return res;
}


const char * strchr(const char *s, int c) {
  do {
    if (c == *s)  return s;
  } while (*s++);
  return 0;
}


int strcmp(const char *dst, const char *src) {
  while ((*dst != 0) && (*dst == *src)) {dst++; src++;}
  return *dst - *src;
}


int strncmp(const char *dst, const char *src, size_t size) {
  if (size == 0) return 0;
  while ((*dst != 0) && (*dst == *src) && --size) {dst++; src++;}
  return *dst - *src;
}


int strspn(const char *s, const char *accept) {
  int res = 0;
  while (s[res] && strchr(accept, s[res])) res++;
  return res;
}

int strcspn(const char *s, const char *reject) {
  int res = 0;
  while (s[res] && !strchr(reject, s[res])) res++;
  return res;
}

END_EXTERN_C
