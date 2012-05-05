/** @file
 * Generic math helper functions.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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


#define union64(HIGH, LOW)          ({ unsigned long long res; asm ("" : "=A"(res) : "d"(HIGH), "a"(LOW)); res; })
#define split64(INPUT, HIGH, LOW)   asm ("" : "=d"(HIGH), "=a"(LOW) : "A"(INPUT));

struct Math
{
  /**
   * We are limited here by the ability to divide through a unsigned
   * long value, thus factor and divisor needs to be less than 1<<32.
   */
  static unsigned long long muldiv128(unsigned long long value, unsigned long factor, unsigned long divisor) {
    unsigned low, high;
    split64(value, high, low);
    unsigned long long lower = static_cast<unsigned long long>(low)*factor;
    unsigned long long upper = static_cast<unsigned long long>(high)*factor;
    unsigned rem = div64(upper, divisor);
    lower += static_cast<unsigned long long>(rem) << 32;
    div64(lower, divisor);

    // this cuts the 96 bits to 64bits
    return (upper << 32) + lower;
  }

  /**
   * Divide a 64bit value through a 32bit value. Returns the remainder.
   */
  static  unsigned div64(unsigned long long &value, unsigned divisor) {
    unsigned vhigh;
    unsigned vlow;
    split64(value, vhigh, vlow);
    unsigned rem  = vhigh % divisor;
    vhigh = vhigh / divisor;
    asm ("divl %2" : "+a"(vlow), "+d"(rem) : "rm"(divisor));
    value = union64(vhigh, vlow);
    return rem;
  }


  /**
   * Divide a 64bit signed value through a 32bit value. Returns the remainder.
   */
  static  int idiv64(long long &value, int divisor) {
    bool sv, sd;
    if ((sv = value < 0))  value = -value;
    if ((sd = divisor < 0))  divisor = -divisor;

    unsigned long long v = value;
    unsigned rem =  div64(v, static_cast<unsigned>(divisor));
    value = v;
    if (sv) rem = -rem;
    if (sv ^ sd) value = -value;

    return rem;
  }
};
