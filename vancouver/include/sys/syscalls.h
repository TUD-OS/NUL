/*
 * NOVA system-call interface.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
#pragma once
#include "hip.h"
#include "utcb.h"

enum
{
  NOVA_IPC_CALL,
  NOVA_IPC_REPLY,
  NOVA_CREATE_PD,
  NOVA_CREATE_EC,
  NOVA_CREATE_SC,
  NOVA_CREATE_PT,
  NOVA_CREATE_SM,
  NOVA_REVOKE,
  NOVA_LOOKUP,
  NOVA_RECALL,
  NOVA_SEMCTL,
  NOVA_ASSIGN_PCI,
  NOVA_ASSIGN_GSI,

  NOVA_FLAG0          = 1 << 4,
  NOVA_CREATE_ECWORK  = NOVA_CREATE_EC | NOVA_FLAG0,
  NOVA_REVOKE_MYSELF  = NOVA_REVOKE | NOVA_FLAG0,
  NOVA_SEMCTL_UP      = NOVA_SEMCTL,
  NOVA_SEMCTL_DOWN    = NOVA_SEMCTL | NOVA_FLAG0,
  NOVA_SEMCTL_DOWN_MULTI = NOVA_SEMCTL_DOWN | (1 << 5)
};

enum ERROR
  {
    NOVA_ESUCCESS = 0,
    NOVA_ETIMEOUT,
    NOVA_ESYS,
    NOVA_ECAP,
    NOVA_EMEM,
    NOVA_EFTR,
    NOVA_ECPU
  };


/**
 * A fast syscall with only two parameters.
 */
static inline unsigned char nova_syscall1(unsigned w0)
{
  asm volatile ("; mov %%esp, %%ecx"
		"; mov $1f, %%edx"
		"; sysenter"
		"; 1:"
		: "+a" (w0) : : "ecx", "edx", "memory");
  return w0;
}

/**
 * Call to the kernel.
 */
static inline unsigned char nova_syscall(unsigned w0, unsigned w1, unsigned w2, unsigned w3, unsigned w4, unsigned *out1=0, unsigned *out2=0)
{
  asm volatile ("push %%ebp;"
		"mov %%ecx, %%ebp;"
		"mov %%esp, %%ecx;"
		"mov $1f, %%edx;"
		"sysenter;"
		"1: ;"
		"pop %%ebp;"
		: "+a" (w0), "+D" (w1), "+S" (w2), "+c"(w4)
		:  "b"(w3)
		: "edx", "memory");
  if (out1) *out1 = w1;
  if (out2) *out2 = w2;
  return w0;
}


inline unsigned char  nova_call(unsigned idx_pt)
{  return nova_syscall1(idx_pt << 8 | NOVA_IPC_CALL); }


inline unsigned char  nova_create_pd (unsigned idx_pd, unsigned utcb, Crd pt_crd, Qpd qpd, unsigned char cpunr)
{  return nova_syscall(idx_pd << 8 | NOVA_CREATE_PD, 0, utcb | cpunr, qpd.value(), pt_crd.value()); }


inline unsigned char  nova_create_ec(unsigned idx_ec, void *utcb, void *esp, unsigned char cpunr, unsigned excpt_base, bool worker)
{
  return nova_syscall(idx_ec << 8 | (worker ? NOVA_CREATE_ECWORK : NOVA_CREATE_EC), 0,
		      reinterpret_cast<unsigned>(utcb) | cpunr, reinterpret_cast<unsigned>(esp), excpt_base); 
}

inline unsigned char  nova_create_sc (unsigned idx_sc, unsigned idx_ec, Qpd qpd)
{  return nova_syscall(idx_sc << 8 | NOVA_CREATE_SC, 0, idx_ec, qpd.value(), 0); }

inline unsigned char  nova_create_pt(unsigned idx_pt, unsigned idx_ec, unsigned long eip, unsigned mtd)
{  return nova_syscall(idx_pt << 8 | NOVA_CREATE_PT, 0, idx_ec, mtd, eip); }

inline unsigned char  nova_create_sm(unsigned idx_sm, unsigned initial=0)
{  return nova_syscall(idx_sm << 8 | NOVA_CREATE_SM, 0, initial, 0, 0); }

inline unsigned char  nova_revoke(Crd crd, bool myself)
{  return nova_syscall(myself ? NOVA_REVOKE_MYSELF : NOVA_REVOKE, crd.value(), 0, 0, 0); }

inline Crd nova_lookup(void *address)
{
  unsigned res;
  Crd crd(reinterpret_cast<unsigned long>(address) >> 12, 32-12, DESC_MEM_ALL);
  if (nova_syscall(NOVA_LOOKUP, crd.value(), 0, 0, 0, &res))
    return Crd(1, 0, 0);
  return Crd(res);
}

inline unsigned char  nova_recall(unsigned idx_ec)
{  return nova_syscall1(idx_ec << 8 | NOVA_RECALL); }


inline unsigned char  nova_semup(unsigned idx_sm)
{  return nova_syscall1(idx_sm << 8 | NOVA_SEMCTL_UP); }


inline unsigned char  nova_semdown(unsigned idx_sm)
{  return nova_syscall1(idx_sm << 8 | NOVA_SEMCTL_DOWN); }

inline unsigned char  nova_semdownmulti(unsigned idx_sm)
{  return nova_syscall1(idx_sm << 8 | NOVA_SEMCTL_DOWN_MULTI); }

inline unsigned char  nova_assign_pci(unsigned pd, unsigned pf_rid, unsigned vf_rid)
{  return nova_syscall(pd << 8 | NOVA_ASSIGN_PCI, pf_rid, vf_rid, 0, 0); }


inline unsigned char  nova_assign_gsi(unsigned idx_sm, unsigned cpu_nr, unsigned rid=0, unsigned long long* msi_address=0, unsigned *msi_value = 0)
{
  unsigned out1;
  unsigned char res = nova_syscall(idx_sm << 8 | NOVA_ASSIGN_GSI, cpu_nr, rid, 0, 0, &out1, msi_value);
  if (msi_address) *msi_address = out1;
  return res;
}
