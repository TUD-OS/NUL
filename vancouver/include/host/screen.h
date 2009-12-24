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
#include <cstring>

class Screen
{
 public:
  /**
   * Put a single char to the VGA monitor.
   */
  static void vga_putc(long value, unsigned short *base, unsigned &pos)
  {
    bool visible = false;
    switch (value & 0xff) 
      {
      case 8: // backspace
	if (pos) pos -=2;
	break;
      case '\n':
	pos += 160 - (pos % 160);
	break;
      case '\r':
	pos -=  pos % 160;
	break;
      case '\t':
	pos +=  16 - (pos % 16);
	break;
      default:
	visible = true;
      }
    // scroll?
    if (pos >= 25*80*2)
      {
	memmove(base, base + 80, 24*80*2);
	memset(base + 24*80, 0, 160);
	pos = 24*80*2;
      }
    if (visible)
      {
	base[pos/2] =  value;
	pos +=2;
      }
  }
};
