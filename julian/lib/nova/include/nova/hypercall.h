/* -*- Mode: C++ -*- */

#pragma once

#include <nova/types.h>
#include <nova/utcb.h>

NOVA_BEGIN

enum {
  // Syscalls
  CALL = 0,
  REPLY, CREATE_PD, CREATE_EC, CREATE_SC, CREATE_PT, CREATE_SM,
  REVOKE, RECALL, SEMCTL,
};

enum {
  // Flags
  NOBLOCK  = 1, NODONATE = 2, NOREPLY  = 4,
  // Implemented combinations
  DCALL    = 0,
  SEND     = NOREPLY | NODONATE,
};

enum {
  // Return values
  SUCCESS = 0,
  TIMEOUT, BAD_SYS, BAD_CAP, BAD_MEM, BAD_FTR,
};

NOVA_INLINE uint8_t hypercall_1(uint8_t syscall_no, uint8_t flags, uint32_t word1)
{
  uint8_t result;

  asm volatile ("mov %%esp, %%ecx\n"
		"mov $0f, %%edx\n"
		"sysenter ; 0:\n"
		: "=a" (result)
		: "a" (syscall_no | (flags << 8)),
		  "D" (word1)
		: "ecx", "edx" );

  return result;
}

NOVA_INLINE uint8_t hypercall_2(uint8_t syscall_no, uint8_t flags, uint32_t word1, uint32_t word2)
{
  uint8_t result;

  asm volatile ("mov %%esp, %%ecx\n"
		"mov $0f, %%edx\n"
		"sysenter ; 0:\n"
		: "=a" (result)
		: "a" (syscall_no | (flags << 8)),
		  "D" (word1), "S" (word2)
		: "ecx", "edx" );

  return result;
}

NOVA_INLINE uint8_t hypercall_3(uint8_t syscall_no, uint8_t flags, uint32_t word1,
                                uint32_t word2, uint32_t word3)
{
  uint8_t result;

  asm volatile ("mov %%esp, %%ecx\n"
		"mov $0f, %%edx\n"
		"sysenter ; 0:\n"
		: "=a" (result)
		: "a" (syscall_no | (flags << 8)),
		  "D" (word1), "S" (word2), "b" (word3)
		: "ecx", "edx" );

  return result;
}

NOVA_INLINE uint8_t hypercall_4(uint8_t syscall_no, uint8_t flags, uint32_t word1,
                                uint32_t word2, uint32_t word3, uint32_t word4)
{
  uint8_t result;

  asm volatile ("push %%ebp\n"
		"mov %%ecx, %%ebp\n"
		"mov %%esp, %%ecx\n"
		"mov $0f, %%edx\n"
		"sysenter ; 0:\n"
		"pop %%ebp\n"
		: "=a" (result),
		  "+c" (word4)
		: "a" (syscall_no | (flags << 8)),
		  "D" (word1), "S" (word2), "b" (word3)
		: "edx" );

  return result;
}

NOVA_INLINE uint8_t call(uint32_t flags, Cap_idx pt, Mtd mtd)
{
  uint8_t result = hypercall_2(CALL, flags, pt, mtd);
  NOVA_MEMCLOBBER;
  return result;
}

NOVA_NORETURN NOVA_INLINE void reply(Mtd mtd, void *esp)
{
  uint8_t result;

  asm volatile ("mov $_abort, %%edx\n"
		"sysenter\n"
		: "=a" (result)
		: "a" (REPLY),
		  // Why is EDI left out? -> was used as conditional 
		  "S" (mtd), "c" (esp));

  NOVA_NOTREACHED;
}

NOVA_INLINE uint8_t create_pd(Cap_idx pd, uint32_t utcb_addr, Qpd qpd, Crd obj, bool vm)
{ return hypercall_4(CREATE_PD, vm ? 1 : 0, pd, utcb_addr, qpd, obj); }

NOVA_INLINE uint8_t create_ec(Cap_idx ec, Utcb *utcb, void *sp)
{ return hypercall_3(CREATE_EC, 0, ec, NOVA_CAST(uint32_t, utcb), NOVA_CAST(uint32_t, sp)); }

NOVA_INLINE uint8_t create_sc(Cap_idx sc, Cap_idx ec, Qpd qpd)
{ return hypercall_3(CREATE_SC, 0, sc, ec, qpd); }

NOVA_INLINE uint8_t create_pt(Cap_idx pt, Cap_idx ec, Mtd mtd, Portal_fn ip)
{ return hypercall_4(CREATE_PT, 0, pt, ec, mtd, NOVA_CAST(uint32_t, ip)); }

NOVA_INLINE uint8_t create_sm(Cap_idx sm, uint32_t count)
{ return hypercall_2(CREATE_SM, 0, sm, count); }

NOVA_INLINE uint8_t recall(Cap_idx ec)
{ return hypercall_1(RECALL, 0, ec); }

NOVA_INLINE uint8_t revoke(Crd caps, bool self)
{
  uint8_t res = hypercall_1(REVOKE, self ? 1 : 0, caps);
  NOVA_MEMCLOBBER;
  return res;
}

NOVA_INLINE uint8_t semup(Cap_idx sm) { return hypercall_1(SEMCTL, 0, sm); }
NOVA_INLINE uint8_t semdown(Cap_idx sm) { return hypercall_1(SEMCTL, 1, sm); }

NOVA_EXTERN_C NOVA_NORETURN NOVA_REGPARM(1) void reply_and_wait_fast(Utcb *utcb);

NOVA_END

/* EOF */
