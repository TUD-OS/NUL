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
