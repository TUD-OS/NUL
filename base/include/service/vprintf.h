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


#pragma once
#include <stdarg.h>

class Vprintf
{
  typedef void (*PutcFunction)(void *data, int value);
  static void put_number(PutcFunction putc, void *data, unsigned long long value, const unsigned base, int pad, bool negative=false);
  static const char * handle_formatstring(PutcFunction putc, void *data, const char *format, va_list &ap);
 public:
  static void vprintf(PutcFunction putc, void *data, const char *format, va_list &ap);
  static void printf(PutcFunction putc, void *data, const char *format, ...) __attribute__ ((format(printf, 3, 4)));
  static void snprintf(char *dst, unsigned size, const char *format, ...) __attribute__ ((format(printf, 3, 4)));
  static void vsnprintf(char *dst, unsigned size, const char *format, va_list &ap);
};
