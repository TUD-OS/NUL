/* -*- Mode: C++ -*- */

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
