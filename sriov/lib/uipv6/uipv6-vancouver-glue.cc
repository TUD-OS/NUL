// -*- Mode: C++ -*-

#include <vmm/timer.h>

static Clock *uipv6_clock;

uint64_t uipv6_vancouver_clock(unsigned freq)
{
  if (uipv6_clock)
    return uipv6_clock->clock(freq);
  else 
    return 0;
}

// EOF
