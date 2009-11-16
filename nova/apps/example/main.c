/* -*- Mode: C -*- */

#include <nova.h>

#define CHECK(x) do { if ((x) != SUCCESS) NOVA_TRAP; } while (0)

int
start(struct HypervisorInfoPage *hip)
{
  Cap_idx user_cap = hip->sel;

  /* Create a semaphore for fun. */
  Cap_idx sem = --user_cap;
  CHECK(create_sm(sem, 0));

  /* Wait for keyboard interrupt and die. */
  CHECK(semdown(hip->pre + 1));

  return 0;
}

/* EOF */
