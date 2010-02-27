/**
 * Logging implementation.
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

#
#include "service/helper.h"
#include "service/logging.h"

void (*Logging::_putcf)(void *, int value);
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
  __exit(0xdeadbeef);
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

void
Logging::hexdump(const void *p, unsigned len)
{
  const unsigned chars_per_row = 16;
  const char *data = reinterpret_cast<const char *>(p);
  const char *data_end = data + len;

  for (unsigned cur = 0; cur < len; cur += chars_per_row) {
    printf("%08x:", cur);
    for (unsigned i = 0; i < chars_per_row; i++)
      if (data+i < data_end)
	Logging::printf(" %02x", ((const unsigned char *)data)[i]);
      else
	printf("   ");
    Logging::printf(" | ");
    for (unsigned i = 0; i < chars_per_row; i++) {
      if (data < data_end)
	printf("%c", ((data[0] >= 32) && (data[0] > 0)) ? data[0] : '.');
      else
	printf(" ");
      data++;
    }
    printf("\n");
  }
}

// EOF
