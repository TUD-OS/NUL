/**
 * Helper functions for MMIO.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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

/**
 * MMIO helper.
 */
class MmioHelper
{
public:
  static void writel(unsigned value, volatile void *address)
  {
    asm volatile ("mov %0,%1" : : "r"(value), "m"(address) : "memory");
  }

  static unsigned readl(volatile void *address)
  {
    unsigned res;
    asm volatile ("mov %1,%0" : "=r"(res) : "m"(address) : "memory");
    return res;
  }
};
