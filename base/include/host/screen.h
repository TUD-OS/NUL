/** @file
 * VGA screen output.
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
#include "service/string.h"

struct Screen
{
  /**
   * Put a single char to the VGA monitor.
   */
  static void vga_putc(long value, unsigned short *base, unsigned &pos)
  {
    if (value < 0) return;
    bool visible = false;
    switch (value & 0xff)
      {
      case 8: // backspace
	if (pos) pos--;
	break;
      case '\n':
	pos += 80 - (pos % 80);
	break;
      case '\r':
	pos -=  pos % 80;
	break;
      case '\t':
	pos +=  8 - (pos % 8);
	break;
      default:
	visible = true;
      }

    // scroll?
    if (pos >= 25*80) {
      memmove(base, base + 80, 24*80*2);
      pos = 24*80;
      for (unsigned i = 0; i < 80; i++) base[pos + i] = 0x0700;
    }
    if (visible) base[pos++] =  value;
  }
};
