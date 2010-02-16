/**
 * Printf implementation.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <bk@vmmon.org>
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


#include <cassert>
#include <cstring>
#include "driver/vprintf.h"
#include "vmm/math.h"

#include <stdio.h>

void
Vprintf::vprintf(PutcFunction putc, void *data, const char *format, va_list &ap)
{
  while (*format)
    {
      switch (*format)
	{
	case '%':
	  format++;
	  format = handle_formatstring(putc, data, format, &ap);
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
  Vprintf::vprintf(putc, data, format, ap);
  va_end(ap);
}

void Vprintf::snprintf(char *dst, unsigned size, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vsnprintf(dst, size, format, ap);
  va_end(ap);
}
