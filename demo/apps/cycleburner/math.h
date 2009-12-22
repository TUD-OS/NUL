/* -*- Mode: C++ -*- */

#pragma once

#include <cstdint>

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
