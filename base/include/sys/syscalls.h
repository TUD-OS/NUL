/*
 * NOVA system-call interface.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2013, Alexander Boettcher <boettcher@tudos.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL.
 *
 * NUL is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 * License version 2 for more details.
 */
#pragma once

#include <nul/compiler.h>
#include <sys/hip.h>
#include <sys/utcb.h>

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
  NOVA_SC_CTL,
  NOVA_PT_CTL,
  NOVA_SEMCTL,
  NOVA_ASSIGN_PCI,
  NOVA_ASSIGN_GSI,

  NOVA_FLAG0          = 1 << 4,
  NOVA_FLAG1          = 1 << 5,
  NOVA_CREATE_ECCLIENT= NOVA_CREATE_EC | NOVA_FLAG0,
  NOVA_REVOKE_MYSELF  = NOVA_REVOKE | NOVA_FLAG0,
  NOVA_SEMCTL_UP      = NOVA_SEMCTL,
  NOVA_SEMCTL_DOWN    = NOVA_SEMCTL | NOVA_FLAG0,
  NOVA_SEMCTL_DOWN_MULTI = NOVA_SEMCTL_DOWN | NOVA_FLAG1,

  NOVA_DEFAULT_PD_CAP = 32,
};

enum ERROR
  {
    NOVA_ESUCCESS = 0,
    NOVA_ETIMEOUT,
    NOVA_EABORT,
    NOVA_ESYS,
    NOVA_ECAP,
    NOVA_EMEM,
    NOVA_EFTR,
    NOVA_ECPU,
    NOVA_EDEV,
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


WARN_UNUSED static inline unsigned char  nova_call(unsigned idx_pt)
{  return nova_syscall1(idx_pt << 8 | NOVA_IPC_CALL); }

static inline unsigned char  nova_create_pd (unsigned idx_pd, Crd pt_crd, unsigned dstpd = NOVA_DEFAULT_PD_CAP)
{  return nova_syscall(idx_pd << 8 | NOVA_CREATE_PD, dstpd, pt_crd.value(), 0, 0); }

static inline unsigned char  nova_create_ec(unsigned idx_ec, void *utcb, void *esp, unsigned char cpunr, unsigned excpt_base, bool worker, unsigned dstpd = NOVA_DEFAULT_PD_CAP)
{
  return nova_syscall(idx_ec << 8 | (worker ? NOVA_CREATE_EC : NOVA_CREATE_ECCLIENT), dstpd,
		      reinterpret_cast<unsigned>(utcb) | cpunr, reinterpret_cast<unsigned>(esp), excpt_base);
}

WARN_UNUSED static inline unsigned char  nova_create_sc (unsigned idx_sc, unsigned idx_ec, Qpd qpd, unsigned cpu, unsigned dstpd = NOVA_DEFAULT_PD_CAP)
{  return nova_syscall(idx_sc << 8 | NOVA_CREATE_SC, dstpd, idx_ec, qpd.value(), cpu); }

WARN_UNUSED static inline unsigned char  nova_ctl_sc(unsigned idx_sc, unsigned long long &computetime)
{  return nova_syscall(idx_sc << 8 | NOVA_SC_CTL, 0, 0, 0, 0, reinterpret_cast<unsigned *>(&computetime) + 1, reinterpret_cast<unsigned *>(&computetime)); }

WARN_UNUSED static inline unsigned char  nova_ctl_pt(unsigned idx_pt, unsigned long id)
{  return nova_syscall(idx_pt << 8 | NOVA_PT_CTL, id, 0, 0, 0); }

WARN_UNUSED static inline unsigned char  nova_create_sm(unsigned idx_sm, unsigned initial=0, unsigned dstpd = NOVA_DEFAULT_PD_CAP)
{  return nova_syscall(idx_sm << 8 | NOVA_CREATE_SM, dstpd, initial, 0, 0); }

static inline unsigned char  nova_revoke(Crd crd, bool myself)
{   return nova_syscall(myself ? NOVA_REVOKE_MYSELF : NOVA_REVOKE, crd.value(), 0, 0, 0); }

static inline unsigned char  nova_revoke_self(Crd crd) { return nova_revoke(crd, true); }
static inline unsigned char  nova_revoke_children(Crd crd) { return nova_revoke(crd, false); }

WARN_UNUSED static inline unsigned char  nova_create_pt(unsigned idx_pt, unsigned idx_ec, unsigned long eip, unsigned mtd, unsigned dstpd = NOVA_DEFAULT_PD_CAP, unsigned long pid = 0)
{
  unsigned char res = nova_syscall(idx_pt << 8 | NOVA_CREATE_PT, dstpd, idx_ec, mtd, eip);
  if (res != NOVA_ESUCCESS)
    return res;

  res = nova_ctl_pt(idx_pt, pid ? pid : idx_pt);

  if (res != NOVA_ESUCCESS)
    nova_revoke(Crd(idx_pt, 0, DESC_CAP_ALL), true);
  else if (!pid)
    nova_revoke(Crd(idx_pt, 0, DESC_TYPE_CAP | DESC_RIGHT_PT_CTRL), true);

  return res;
}

inline Crd nova_lookup(Crd crd)
{
  unsigned res = 0;
  nova_syscall(NOVA_LOOKUP, crd.value(), 0, 0, 0, &res);
  return Crd(res);
}

static inline Crd nova_lookup(void *m)
{
  return nova_lookup(Crd(reinterpret_cast<mword>(m) >> 12, 32 - 12, DESC_MEM_ALL));
}

WARN_UNUSED static inline unsigned char  nova_recall(unsigned idx_ec)
{  return nova_syscall1(idx_ec << 8 | NOVA_RECALL); }


WARN_UNUSED static inline unsigned char  nova_semup(unsigned idx_sm)
{  return nova_syscall1(idx_sm << 8 | NOVA_SEMCTL_UP); }


WARN_UNUSED static inline unsigned char  nova_semdown(unsigned idx_sm)
{  return nova_syscall1(idx_sm << 8 | NOVA_SEMCTL_DOWN); }

WARN_UNUSED static inline unsigned char  nova_semdownmulti(unsigned idx_sm)
{  return nova_syscall1(idx_sm << 8 | NOVA_SEMCTL_DOWN_MULTI); }

WARN_UNUSED static inline unsigned char  nova_assign_pci(unsigned pd, void *pf_cfg_mem, unsigned vf_rid)
{  return nova_syscall(pd << 8 | NOVA_ASSIGN_PCI, reinterpret_cast<unsigned>(pf_cfg_mem), vf_rid, 0, 0); }


WARN_UNUSED static inline unsigned char  nova_assign_gsi(unsigned idx_sm, unsigned cpu_nr, void *pci_cfg_mem=0, unsigned long long* msi_address=0, unsigned *msi_value = 0)
{
  unsigned out1;
  unsigned char res = nova_syscall(idx_sm << 8 | NOVA_ASSIGN_GSI, reinterpret_cast<unsigned>(pci_cfg_mem), cpu_nr, 0, 0, &out1, msi_value);
  if (msi_address) *msi_address = out1;
  return res;
}

// EOF
