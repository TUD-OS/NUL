/* -*- Mode: C -*- */

#include <contiki-conf.h>

extern uint64_t uipv6_vancouver_clock(unsigned freq);

clock_time_t
clock_time(void)
{
  return uipv6_vancouver_clock(CLOCK_CONF_SECOND);
}

unsigned long
clock_seconds(void)
{
  return clock_time() / CLOCK_CONF_SECOND;
}

/* EOF */
