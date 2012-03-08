/** @file
 * Fixed-width integer types.
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

/* Include stddef to get proper definition of NULL. */
#include <stddef.h>

#include <nul/compiler.h>

#if !defined(__i386) || !defined(__GNUC__)
#error Your platform is not supported.
#endif

BEGIN_EXTERN_C
#ifdef __MMX__
#include <mmintrin.h>
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __SSSE3__
#include <tmmintrin.h>
#endif
END_EXTERN_C

/* Constant-width integer types. */
typedef unsigned long long uint64;
typedef unsigned int       uint32;
typedef unsigned short     uint16;
typedef unsigned char      uint8;
typedef unsigned long      mword;

typedef signed long long int64;
typedef signed int       int32;
typedef signed short     int16;
typedef signed char      int8;

/* NUL specific types */

typedef unsigned log_cpu_no;
typedef unsigned phy_cpu_no;
typedef unsigned cap_sel;       /* capability selector */

/* EOF */
