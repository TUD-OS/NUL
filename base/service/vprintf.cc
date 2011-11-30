/** @file
 * Printf implementation.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */


#include "service/helper.h"
#include "service/string.h"
#include "service/vprintf.h"
#include "service/math.h"

/**
 * Output a number with base.
 */
void Vprintf::put_number(PutcFunction putc, void *data, unsigned long long value, const unsigned base, int pad, bool negative)
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



const char * Vprintf::handle_formatstring(PutcFunction putc, void *data, const char *format, va_list &ap)
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
	case 'z':		// size_t modifier
	  l = 0;
	  format++;
	  break;
  case '#': // write 0x before numbers
	  putc(data, '0');
	  putc(data, 'x');
    format++;
    break;
	case 'l':
	  l++;
	  format++;
	  break;
	case '.':
	  if (format[1] == '*') {
	    pad = 2 * va_arg(ap, unsigned);
	    format++;
	  }
	  format++;
	  break;
	case 's':
	  {
	    const unsigned char *s = va_arg(ap, const unsigned char *);
	    if (!s)
	      s = reinterpret_cast<const unsigned char *>("<null>");
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
	  putc(data, va_arg(ap, unsigned));
	  return ++format;
	case '%':
	  putc(data, *format & 0xff);
	  return ++format;
	case 0:
	  return format;
	default:
	  assert(!"invalid format character");
	}
    }
  return format;
}



struct snprintf_data {
  char *ptr;
  unsigned size;
};


static void snprintf_putc(void *data, int value)
{
  snprintf_data *d = reinterpret_cast<snprintf_data *>(data);

  if (value == -2 && d->size) *d->ptr = 0;
  if (value < 0) return;
  switch (d->size)
    {
    case 1:
      value = 0;
    default:
      *d->ptr++ = value;
      d->size--;
    case 0:
      break;
    }
}



void Vprintf::vsnprintf(char *dst, unsigned size, const char *format, va_list &ap)
{
  snprintf_data data;
  data.ptr = dst;
  data.size = size;
  vprintf(snprintf_putc, &data, format, ap);
}

void Vprintf::snprintf(char *dst, unsigned size, const char *format, ...)
{
  snprintf_data data;
  data.ptr = dst;
  data.size = size;
  va_list ap;
  va_start(ap, format);
  vprintf(snprintf_putc, &data, format, ap);
  va_end(ap);
}


void Vprintf::vprintf(PutcFunction putc, void *data, const char *format, va_list &ap)
{
  putc(data, -1);
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
  putc(data, -2);
}


void
Vprintf::printf(PutcFunction putc, void *data, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vprintf(putc, data, format, ap);
  va_end(ap);
}
