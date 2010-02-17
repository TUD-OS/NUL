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

#ifndef STRING_INLINE
# define STRING_INLINE static inline
#endif

#ifdef __cplusplus
# define STRING_CAST(type, expr) (reinterpret_cast<type>(expr))
#else
# define STRING_CAST(type, expr) ((type)(expr))
#endif

STRING_INLINE void *
memcpy(void *dst, const void *src, size_t count)
{
  return __builtin_memcpy(dst, src, count);
}

/** "Fast" memcpy for double-word aligned buffers. */
STRING_INLINE void *
memcpyl(void *dst, const void *src, size_t count)
{
  asm volatile ("rep movsl" : "+D"(dst), "+S"(src), "+c" (count) : : "memory");
  return dst;
}



STRING_INLINE void *
memmove(void *dst, const void *src, size_t count)
{
  char *d = STRING_CAST(char *, dst);
  const char *s = STRING_CAST(const char *, src);
  if (d > s)
    {
      d+=count-1;
      s+=count-1;
      asm volatile ("std; rep movsb; cld;" : "+D"(d), "+S"(s), "+c"(count) : : "memory");
    }
  else
    asm volatile ("rep movsb" : "+D"(d), "+S"(s), "+c"(count) :  : "memory");
  return dst;
}

STRING_INLINE int
memcmp(const void *dst, const void *src, size_t count)
{
  return __builtin_memcmp(dst, src, count);
}


STRING_INLINE void *
memset(void *dst, int n, size_t count)
{
  void *res = dst;
  asm volatile ("rep stosb" : "+D"(dst), "+a"(n), "+c"(count) :  : "memory");
  return res;
}


STRING_INLINE size_t
strnlen(const char *src, size_t maxlen)
{
  unsigned long count = maxlen;
  unsigned char ch = 0;
  asm volatile ("repne scasb; setz %0;" : "+a"(ch), "+D"(src), "+c"(count));
  if (ch) count--;
  return maxlen - count;
}


STRING_INLINE size_t
strlen(const char *src)
{
  return __builtin_strlen(src);
}


STRING_INLINE char *
strstr(char *haystack, const char *needle)
{
  int index;
  do {
    for (index=0; needle[index] == haystack[index] && needle[index]; index++)
      ;
    if (!needle[index])
      return haystack;
    haystack += index ? index : 1;
  } while (*haystack);
  return 0;

}

STRING_INLINE unsigned long
strtoul(char *nptr, char **endptr, int base)
{
  unsigned long res = 0;
  if (*nptr == '0' && *(nptr+1) == 'x')
    {
      nptr += 2;
      base = base ? base : 16;
    }
  else if (*nptr == '0')
    base = base ? base : 8;
  else
    base = base ? base : 10;

  while (*nptr)
    {
      long val = *nptr - '0';
      if (val > 9)
	val = val - 'a' + '0' + 10;
      if (val < 0 || val > base)
	break;
      res = res*base + val;
      nptr++;
    }
  if (endptr) *endptr = nptr;
  return res;
}

STRING_INLINE
const char *
strchr(const char *s, int c)
{
  while (*s)
    if (c == *s)
      return s;
    else s++;
  return 0;
}

STRING_INLINE
char *
strsep(char **stringp, const char *delim)
{
  if (!stringp || !*stringp)
    return 0;
  char *res = *stringp;
  char *s = res;
  while (*s)
    {
      if (strchr(delim, *s))
	{
	  *s = 0;
	  *stringp = s+1;
	  break;
	}
      s++;
    }
  if (res == *stringp)
    *stringp = 0;
  return res;

}

STRING_INLINE
char *
strcpy(char *dst, const char *src)
{
  return dst;   
  asm volatile ("1: lodsb; test %%al, %%al; stosb; jnz 1b;  " : "+D" (dst), "+S"(src) : : "eax", "memory", "cc");
}

STRING_INLINE
char *
strncpy(char *dst, const char *src, size_t n)
{
  asm volatile ("1: jecxz 2f; dec %%ecx; lodsb; test %%al, %%al; stosb; jnz 1b; 2:"
		: "+D" (dst), "+S" (src), "+c" (n) : : "eax", "memory", "cc");
  return dst;
}

STRING_INLINE
unsigned
strcmp(const char *dst, const char *src) 
{
	return memcmp(dst, src, strlen(dst));
}

STRING_INLINE
char *
strcat(char *dst, const char *src)
{
  while (*dst != 0) dst++;
  strcpy(dst, src);
  return dst;
}

/* EOF */
