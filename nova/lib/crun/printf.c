/* -*- Mode: C -*- */

/**
 * Printf implementation.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <kauer@tudos.org>
 *
 * This file is part of Florence2.
 *
 * Florence2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Florence2 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

/* Ported to C by Julian Stecklina. */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* Disable assertions. */
#define assert(x)

/* Allow others to implement optimized puts versions. */
__attribute__((weak)) int
puts(const char *s)
{
  while (*s != 0)
    putchar(*s++);
  putchar('\n');
  return 1;
}

/**
 * Output a number with base.
 */
static void
put_number(putchar_fn put, void *data, unsigned long long value, const unsigned base, int pad, bool negative)
{
  assert(base<=36 && base>=2);

  unsigned char buffer[32];
  int size = 0;
  do {
    unsigned mod = value % base; value /= base;
    if (mod>=10)  mod += 0x27;
    assert(size < 32);
    buffer[size++] = mod+0x30;
  } while (value);

  char ch = pad & 1 ? '0' : ' ';
  pad >>= 1;
  if (negative && pad)  pad --;
  while (size < pad--)
    put(data, ch);
  if (negative) put(data, '-');
  while (size--)
    put(data, buffer[size]);
}

const char *
handle_formatstring(putchar_fn put, void *data, const char *format, va_list *ap)
{
  unsigned l=0;
  int pad = 0;
  while (*format) {
    switch (*format) {
    case '0':
      if (pad == 0)
	pad = 1;
      else
	pad = (pad/2)*20 | (pad & 1);
      format++;
      break;
    case '1'...'9':
      pad = (2*(*format - '0')) + (pad/2)*20 + (pad & 1);
      format++;
      break;
    case 'l':
      l++;
      format++;
      break;
    case 's':
      {
	const char *s = va_arg(*ap, const char *);
	if (!s)
	  s = "<null>";
	if (!pad)
	  pad = -1;
	pad>>=1;
	while (*s && pad-- ) put(data, *s++);
	while (pad-- > 0 && pad < 256)  put(data, ' ');
      }
      return ++format;
    case 'p':
      put(data, '0'); put(data, 'x');
    case 'x':
      if (l==2)
	put_number(put, data, va_arg(*ap, unsigned long long), 16, pad, false);
      else
	put_number(put, data, va_arg(*ap, unsigned long), 16, pad, false);
      return ++format;
    case 'd':
      {
	long long a;
	if (l==2)
	  a = va_arg(*ap, long long);
	else
	  a = va_arg(*ap, long);
	bool negative = a < 0;
	if (negative) a=-a;
	put_number(put, data, a, 10, pad, negative);
	return ++format;
      }
    case 'u':
      if (l==2)
	put_number(put, data, va_arg(*ap, unsigned long long), 10, pad, false);
      else
	put_number(put, data, va_arg(*ap, unsigned long), 10, pad, false);
      return ++format;
    case 'c':
      put(data, va_arg(*ap, int));
      return ++format;
    case '%':
      put(data, *format);
      return ++format;
    case 'z':		// size_t
      l = 0; format++; break;
    case 0:
      return format;
    default:
      put(data, '?'); return ++format;
    }
  }
  return format;
}

int
vprintf(const char *format, va_list ap)
{
  int total_chars = 0;
  
  void count_putchar(void *data, int c) { total_chars++; putchar(c); }

  while (*format) {
    switch (*format) {
    case '%':
      format++;
      format = handle_formatstring(count_putchar, NULL, format, &ap);
      break;
    default:
      count_putchar(NULL, *format++);
    }
  }

  return total_chars;
}

int
vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
  __label__ done;
  int total_chars = 0;

  void limit_putchar(void *data, int c) {
    if (size - total_chars <= 1)
      goto done;
    total_chars++;
    *(str++) = c;
  }

  while (*format) {
    switch (*format) {
    case '%':
      format++;
      format = handle_formatstring(limit_putchar, NULL, format, &ap);
      break;
    default:
      limit_putchar(NULL, *format++);
    }
  }

 done:
  if (size - total_chars >= 1)
    *str = 0;

  return total_chars;
}

int
printf(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int res = vprintf(format, ap);
  va_end(ap);
  return res;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int res = vsnprintf(str, size, format, ap);
  va_end(ap);
  return res;
}

int sprintf(char *str, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int res = vsnprintf(str, ~0UL, format, ap);
  va_end(ap);
  return res;
}

/* EOF */
