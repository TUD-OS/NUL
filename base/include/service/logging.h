/** @file
 * Logging support.
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
#include "vprintf.h"

class Logging : public Vprintf
{
  static void (*_putcf)(void *data, int value);
  static void *_data;
 public:
  // debug functions
  static void panic(const char *format, ...) __attribute__((noreturn))  __attribute__ ((format(printf, 1, 2)));
  static void printf(const char *format, ...) __attribute__ ((format(printf, 1, 2)));
  static void vprintf(const char *format, va_list &ap);
  static void init(void (*putcf)(void *, int), void *data) { _putcf = putcf; _data = data; }
};
