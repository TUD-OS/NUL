/**
 * @file 
 * UUID (Universally Unique Identifier) class
 *
 * Copyright (C) 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

// POD structure required for inclusion into packed structures
typedef struct uuid_pod {
    unsigned char id[16];
} uuid_t;

class Uuid : protected uuid_pod {
public:
  Uuid(uuid_t &x) { memcpy(id, x.id, sizeof(id)); }
  void as_text(char *dest);
};

void Uuid::as_text(char *dest)
{
  const int chunks[] = { -4, -2, -2, 2, 6 };
  const int *chunk = chunks;
  unsigned start = 0, len = 4;
  for (unsigned i = 0; i < 16; i++) {
    if (i == start + len) {
      *dest++ = '-';
      chunk++;
      start = i;
      len = (*chunk > 0) ? *chunk : -*chunk;
    }
    unsigned char ch = (*chunk > 0) ? id[i] : id[start+len - (i - start) - 1];
    Vprintf::snprintf(dest, 3, "%02x", ch);
    dest += 2;
  }
  *dest = '\0';
}


