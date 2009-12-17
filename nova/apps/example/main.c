/* -*- Mode: C -*- */

#include <nova.h>
#include <stdio.h>

#define CHECK(x) do { if ((x) != SUCCESS) NOVA_TRAP; } while (0)

static uint32_t test_stack[32/4] NOVA_ALIGN(4096) = { 1 };
static uint32_t *test_stack_top = &test_stack[sizeof(test_stack)/sizeof(uint32_t)];

static Cap_idx self_map_pt;
static Utcb   *self_map_utcb;
static Utcb   *root_utcb;

void _abort();

int putchar(int c)
{
  /* Dummy */
  return c;
}

static void *
self_map_mem(uint32_t addr, size_t length)
{
  /* XXX b0rken */

  utcb_add_mappings(root_utcb, false, addr, length, addr, 0x3<<2 | CRD_MEM);
  

  return NULL;
}

int
start(struct Hip *hip)
{
  root_utcb = ((Utcb *)hip)-1;

  printf("Example Roottask\n");


  Cap_idx user_cap = hip->sel;

  /* Create a semaphore for fun. */
  Cap_idx sem = --user_cap;
  CHECK(create_sm(sem, 0));

  Cap_idx test_pt = --user_cap;
  Cap_idx test_ec = --user_cap;
  //Cap_idx test_sc = --user_cap;
  Utcb *test_utcb = (Utcb *)0x200000; /* See roottask.ld */
  /* XXX Allow mappings */

  test_stack_top[-1] = (uint32_t)test_utcb;
  CHECK(create_ec(test_ec, test_utcb, test_stack_top-1));
  CHECK(create_pt(test_pt, test_ec, empty_message(), (Portal_fn)reply_and_wait_fast));
  //CHECK(create_sc(test_sc, test_ec, qpd(2, 10000)));
  
  self_map_pt = test_pt;
  self_map_utcb = test_utcb;

  CHECK(call(CALL, test_pt, empty_message()));

  printf("Waiting for keyboard interrupt to crash spectacularly.\n");
  CHECK(semdown(hip->pre + 1));

  return 0;
}

/* EOF */
