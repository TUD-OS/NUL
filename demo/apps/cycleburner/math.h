// -*- Mode: C++ -*-
/** @file
 * Math helpers
 *
 * Copyright (C) 2009, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include "nul/types.h"

typedef signed char  int8_t;
typedef signed short int16_t;
typedef uint8  uint8_t;
typedef uint16 uint16_t;
typedef uint32 uint32_t;

# define M_PI		3.14159265358979323846

static inline float
fsin(float f)
{
  asm ("fsin\n" : "+t" (f));
  return f;
}

static inline float
fsqrt(float f)
{
  asm ("fsqrt\n" : "+t" (f));
  return f;
}


/* EOF */
