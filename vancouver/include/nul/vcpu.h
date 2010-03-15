/**
 * External Virtual CPU interface.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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
#include "bus.h"
#include "message.h"

struct CpuMessage {
  enum Type {
    TYPE_CPUID,
    TYPE_CPUID_WRITE,
    TYPE_RDTSC,
    TYPE_RDMSR,
    TYPE_WRMSR,
    TYPE_TRIPLE,
    TYPE_INIT,
    TYPE_HLT,
    TYPE_CHECK_IRQ,
    TYPE_SINGLE_STEP,
  } type;
  union {
    struct {
      CpuState *cpu;
      unsigned  cpuid_index;
    };
    struct {
      unsigned nr;
      unsigned reg;
      unsigned mask;
      unsigned value;
    };
  };
  CpuMessage(Type _type, CpuState *_cpu) : type(_type), cpu(_cpu) { if (type == TYPE_CPUID) cpuid_index = cpu->eax; }
  CpuMessage(unsigned _nr, unsigned _reg, unsigned _mask, unsigned _value) : type(TYPE_CPUID_WRITE), nr(_nr), reg(_reg), mask(_mask), value(_value) {}
};


class VCpu
{
  VCpu *_last;

protected:
  enum {
    HAZARD_IRQ    = 1,
    HAZARD_INHLT  = 2,
    HAZARD_INIT   = 4,
  };
  volatile unsigned hazard;
  unsigned lastmsi;

public:
  DBus<CpuMessage>       executor;
  DBus<MessageMem>       mem;
  DBus<MessageMemRegion> memregion;

  VCpu *get_last() { return _last; }
  bool set_cpuid(unsigned nr, unsigned reg, unsigned value, unsigned invmask=~0) {  CpuMessage msg(nr, reg, ~invmask, value & invmask); return executor.send(msg); }

  VCpu (VCpu *last) : _last(last) {}
};
