/* XXX Major hackery. */

#define STRING_INLINE

/* We don't need extern "C", because we compile the non-inline version
   from C only. For the inline version, it does not matter. */
#include <string.h>

void *
memcpy(void *dst, const void *src, size_t count)
{
  void *ret = dst;
  asm volatile ("rep movsb" : "+D"(dst), "+S"(src), "+c" (count) : : "memory");
  return ret;
}

void *
memmove(void *dst, const void *src, size_t count)
{
  char *d = dst;
  const char *s = src;
  if (d > s) {
    d+=count-1;
    s+=count-1;
    asm volatile ("std; rep movsb; cld;" : "+D"(d), "+S"(s), "+c"(count) : : "memory");
  } else
    asm volatile ("rep movsb" : "+D"(d), "+S"(s), "+c"(count) :  : "memory");
  return dst;
}

void *
memset(void *dst, int n, size_t count)
{
  void *res = dst;
  asm volatile ("rep stosb" : "+D"(dst), "+a"(n), "+c"(count) :  : "memory");
  return res;
}

size_t
strnlen(const char *src, size_t maxlen)
{
  unsigned long count = maxlen;
  unsigned char ch = 0;
  asm volatile ("repne scasb; setz %0;" : "+a"(ch), "+D"(src), "+c"(count));
  if (ch) count--;
  return maxlen - count;
}

size_t
strlen(const char *src) { return strnlen(src, ~0ul); }


char *
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

unsigned long
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

const char *
strchr(const char *s, int c)
{
  while (*s)
    if (c == *s)
      return s;
    else s++;
  return 0;
}

char *
strsep(char **stringp, const char *delim)
{
  if (!stringp || !*stringp)
    return 0;
  char *res = *stringp;
  char *s = res;
  while (*s) {
    if (strchr(delim, *s)) {
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

int
memcmp(const void *dst, const void *src, size_t count)
{
  const char *d = dst;
  const char *s = src;
  unsigned i;
  for (i=0; i < count; i++)
    if (s[i] > d[i])
      return 1;
    else if (s[i] < d[i])
      return -1;
  return 0;
}

char *
strcpy(char *dst, const char *src)
{
  char *r = dst;
  asm volatile ("1: lodsb; test %%al, %%al; stosb; jnz 1b;  " : "+D" (dst), "+S"(src) : : "eax", "memory", "cc");
  return r;
}

char *
strncpy(char *dst, const char *src, size_t n)
{
  char *r = dst;
  asm volatile ("1: jecxz 2f; dec %%ecx; lodsb; test %%al, %%al; stosb; jnz 1b; 2:"
		: "+D" (dst), "+S" (src), "+c" (n) : : "eax", "memory", "cc");
  return r;
}

char *
strcat(char *dst, const char *src)
{
  char *r = dst;
  while (*dst != 0) dst++;
  strcpy(dst, src);
  return r;
}


int
strcmp(const char *dst, const char *src)
{
  return memcmp(dst, src, strlen(dst));
}

/* EOF */
