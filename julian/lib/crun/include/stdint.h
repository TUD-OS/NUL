/* -*- Mode: C -*- */

#pragma once

#include <stddef.h>

#if defined(__i386) && defined(__GNUC__)

# if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >= 40500
# include_next <stdint.h>
# else
/* GCC prior to 4.5 do not include stdint.h */

typedef unsigned long long uint64_t;
typedef unsigned int       uint32_t;
typedef unsigned short     uint16_t;
typedef unsigned char      uint8_t;

typedef uint32_t           uintptr_t;

typedef signed long long   int64_t;
typedef signed int         int32_t;
typedef signed short       int16_t;
typedef signed char        int8_t;
# endif

#else
#error Your platform is not (yet?) supported.
#endif
