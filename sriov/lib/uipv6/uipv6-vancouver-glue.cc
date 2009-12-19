// -*- Mode: C++ -*-

#include <driver/logging.h>
#include <vmm/timer.h>
#include <stdint.h>

Clock *uipv6_clock;

extern "C" uint64_t uipv6_vancouver_clock(unsigned freq)
{
  if (uipv6_clock)
    return uipv6_clock->clock(freq);
  else 
    return 0;
}

extern "C" void uip_log(char *m)
{
  Logging::printf("%s", m);
}

// EOF
