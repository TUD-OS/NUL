/** @file
 * Logging implementation.
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

#
#include "service/helper.h"
#include "service/logging.h"

void dummy_putcf(void *, int) {}
void (*Logging::_putcf)(void *, int value) = dummy_putcf;
void *Logging::_data;

void
Logging::panic(const char *format, ...)
{
  va_list ap;
  Vprintf::printf(_putcf, _data, "\nPANIC: ");
  va_start(ap, format);
  Vprintf::vprintf(_putcf, _data, format, ap);
  va_end(ap);
  Vprintf::printf(_putcf, _data, "\n");
  do_exit(format);
}


void
Logging::printf(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  Vprintf::vprintf(_putcf, _data, format, ap);
  va_end(ap);
}

void
Logging::vprintf(const char *format, va_list &ap)
{
  Vprintf::vprintf(_putcf, _data, format, ap);
}
