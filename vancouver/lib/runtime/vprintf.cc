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


#include <cassert>
#include <cstring>
#include "driver/vprintf.h"
#include "vmm/math.h"


/**
 * Output a number with base.
 */
void
Vprintf::put_number(PutcFunction putc, void *data, unsigned long long value, const unsigned base, int pad, bool negative)
{
  assert(base<=36 && base>=2);

  unsigned char buffer[32];
  int size = 0;
  do
    {
      unsigned mod = Math::div64(value, base);
      if (mod>=10)  mod += 0x27;
      assert(size < 32);
      buffer[size++] = mod+0x30;
    }
  while (value);

  char ch = pad & 1 ? '0' : ' ';
  pad >>= 1;
  if (negative && pad)  pad --;
  while (size < pad--)
    putc(data, ch);
  if (negative) putc(data, '-');
  while (size--)
    putc(data, buffer[size]);
}



const char *
Vprintf::handle_formatstring(PutcFunction putc, void *data, const char *format, va_list &ap)
{
  unsigned l=0;
  int pad = 0;
  while (*format)
    {
      switch (*format)
	{
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
	    const char *s = va_arg(ap, const char *);
	    if (!s)
	      s = "<null>";
	    if (!pad)
	      pad = -1;
	    pad>>=1;
	    while (*s && pad-- ) putc(data, *s++);
	    while (pad-- > 0 && pad < 256)  putc(data, ' ');
	  }
	  return ++format;
	case 'p':
	  putc(data, '0');
	  putc(data, 'x');
	case 'x':
	  if (l==2)
	    put_number(putc, data, va_arg(ap, unsigned long long), 16, pad);
	  else
	    put_number(putc, data, va_arg(ap, unsigned long), 16, pad);
	  return ++format;
	case 'd':
	  {
	    long long a;
	    if (l==2)
	      a = va_arg(ap, long long);
	    else
	      a = va_arg(ap, long);
	    bool negative = a < 0;
	    if (negative) a=-a;
	    put_number(putc, data, a, 10, pad, negative);
	    return ++format;
	  }
	case 'u':
	  if (l==2)
	    put_number(putc, data, va_arg(ap, unsigned long long), 10, pad);
	  else
	    put_number(putc, data, va_arg(ap, unsigned long), 10, pad);
	  return ++format;
	case 'c':
	  putc(data, va_arg(ap, int));
	  return ++format;
	case '%':
	  putc(data, *format);
	  return ++format;
	case 0:
	  return format;
	default:
	  assert(!"invalid format character");
	}
    }
  return format;
}

void
Vprintf::vprintf(PutcFunction putc, void *data, const char *format, va_list &ap)
{
  while (*format)
    {
      switch (*format)
	{
	case '%':
	  format++;
	  format = handle_formatstring(putc, data, format, ap);
	  break;
	default:
	  putc(data, *format++);
	}
    }
}


void
Vprintf::printf(PutcFunction putc, void *data, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vprintf(putc, data, format, ap);
  va_end(ap);
}
