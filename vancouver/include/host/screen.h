/**
 * VGA screen output.
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

#pragma once
#include "service/string.h"

class Screen
{
 public:

  /**
   * Scrolls the VGA screen contents up. Does not update cursor position.
   */
  static void vga_scroll_up(unsigned short *base, unsigned rows,
			    unsigned char attr)
  {
    if (rows > 25) rows = 25;
    memmove(base, base + (rows*80), (25-rows)*80*2);
    unsigned short *clear = base + (25 - rows)*80;
    for (unsigned i = 0; i < 80*rows; i++)
      clear[i] = attr << 8;
  }

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
	vga_scroll_up(base, 1, 0x07);
	pos = 24*80;
    }
    if (visible) base[pos++] =  value;
  }
};
