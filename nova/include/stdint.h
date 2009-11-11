/* -*- Mode: C -*- */

#pragma once

#ifdef __i386
typedef unsigned long long uint64_t;
typedef unsigned int       uint32_t;
typedef unsigned short     uint16_t;
typedef unsigned char      uint8_t;

typedef long long int64_t;
typedef int       int32_t;
typedef short     int16_t;
typedef char      int8_t;

#else
#error Your platform is not (yet?) supported.
#endif

/* EOF */
