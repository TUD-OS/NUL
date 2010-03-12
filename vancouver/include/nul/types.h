/* -*- Mode: C -*- */

#pragma once

/* Include stddef to get proper definition of NULL. */
#include <stddef.h>

#if !defined(__i386) || !defined(__GNUC__)
#error Your platform is not supported.
#endif

/* Constant-width integer types. */
typedef unsigned long long uint64;
typedef unsigned int       uint32;
typedef unsigned short     uint16;
typedef unsigned char      uint8;
typedef unsigned int       mword;

/* EOF */
