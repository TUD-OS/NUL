/**
 * Generic cpu state.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
#pragma once
#include "sys/utcb.h"
#include "service/helper.h"

/**
 * A generic cpu state class.
 */
class CpuState : public Utcb
{
 public:
  unsigned cpl()   { return (ss.ar >> 5) & 3; }
  unsigned iopl()  { return (efl >> 12) & 3; }
  unsigned pm()    { return cr0 & 0x1; }
  unsigned pg()    { return cr0 & 0x80000000; }
  unsigned v86()   { return cr0 & 0x1 && efl & (1 << 17); }
  void edx_eax(unsigned long long value)
  {
    eax = value;
    edx = value >> 32;
  };

  unsigned long long edx_eax() {  return static_cast<unsigned long long>(edx) << 32 | eax; };
  void GP0() {
    assert(~inj_info & 0x80000000);
    inj_info = 0x80000b0d | (inj_info & INJ_IRQWIN);
    inj_error = 0;
  }
};

#define assert_mtr(value) { assert((cpu->head.mtr.untyped() & (value)) == (value)); }



class InstructionCache;
class KernelSemaphore;
/**
 * Some state per VCPU that is not available in the above CPU state.
 *
 * We split these two cases to avoid copy/in-out or extra dereferencing cost.
 */
class VirtualCpuState
{
 public:
  enum Hazards
  {
    HAZARD_IRQ     = 1u << 0,
    HAZARD_INHLT   = 1u << 1,
    HAZARD_INIT    = 1u << 2,
    HAZARD_BIOS    = 1u << 4,
    HAZARD_CRWRITE = 1u << 5,
    HAZARD_STIBLOCK= 1u << 6,
    HAZARD_CTRL    = 1u << 7,
  };

  volatile unsigned hazard;

  // block+recall
  unsigned         cap_vcpu;
  KernelSemaphore *block_sem;

  // tracks read+write access to the CPU state
  unsigned mtr_read;
  unsigned mtr_write;

  // tracks errors during execution
  int fault;
  unsigned error_code;
  int debug_fault_line;

  // old state to save for instruction abort
  unsigned oeip;
  unsigned oesp;
  unsigned oactv_state;

  // instruction and data cache
  InstructionCache *instcache;

  // pdpt cache for 32-bit PAE
  unsigned long long pdpt[4];

  // debug register
  unsigned dr[4];
  unsigned dr6;

  // MSR state
  unsigned long msr_efer;

  unsigned lastmsi;

  // x87, MMX, SSE2, SSE3
  unsigned fpustate[512/sizeof(unsigned)] __attribute__((aligned(16)));
};
