/* -*- Mode: C -*- */

#include <nova.h>
#include <stdio.h>

#define CHECK(x) do { if ((x) != SUCCESS) NOVA_TRAP; } while (0)

uint32_t test_stack[4096/4] NOVA_ALIGN(4096) = { 1 };

NOVA_REGPARM(0) void test_entry(uint32_t arg)
{
  asm volatile ("mov %%eax, %%esi\n"
		"ud2a\n"
		::"a" (arg));
}

void _abort();

int putchar(int c)
{
  /* Dummy */
  return c;
}

int
start(struct Hip *hip)
{
  printf("Example Roottask\n");

  Cap_idx user_cap = hip->sel;

  /* Create a semaphore for fun. */
  Cap_idx sem = --user_cap;
  CHECK(create_sm(sem, 0));

  Cap_idx test_pt = --user_cap;
  Cap_idx test_ec = --user_cap;
  Cap_idx test_sc = --user_cap;
  Utcb *test_utcb = (Utcb *)0x200000; /* See roottask.ld */

  test_stack[1023] = 0xCAFE;
  test_stack[1022] = (uint32_t)&_abort;
  

  CHECK(create_ec(test_ec, test_utcb, &test_stack[1022]));
  CHECK(create_pt(test_pt, test_ec, empty_message(), (Portal_fn)test_entry));
  CHECK(create_sc(test_sc, test_ec, qpd(2, 10000)));

  CHECK(call(SEND, test_pt, empty_message()));

  printf("Waiting for keyboard interrupt to crash spectacularly.\n");
  CHECK(semdown(hip->pre + 1));

  return 0;
}

/* EOF */
