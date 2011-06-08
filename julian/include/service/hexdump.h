/** @file
 * General-purpose hexadecimal dump.
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include <nul/types.h>

static inline
void hexdump(const void *p, unsigned len)
{
  const unsigned chars_per_row = 16;
  const char *data = reinterpret_cast<const char *>(p);
  const char *data_end = data + len;

  for (unsigned cur = 0; cur < len; cur += chars_per_row) {
    Logging::printf("%08x:", cur);
    for (unsigned i = 0; i < chars_per_row; i++)
      if (data+i < data_end)
	Logging::printf(" %02x", reinterpret_cast<const unsigned char *>(data)[i]);
      else
	Logging::printf("   ");
    Logging::printf(" | ");
    for (unsigned i = 0; i < chars_per_row; i++) {
      if (data < data_end)
	Logging::printf("%c", ((data[0] >= 32) && (data[0] > 0)) ? data[0] : '.');
      else
	Logging::printf(" ");
      data++;
    }
    Logging::printf("\n");
  }
}

// EOF
