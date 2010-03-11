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

struct CpuMessage {
  enum Type {
    TYPE_CPUID,
    TYPE_CPUID_WRITE,
    TYPE_RDTSC,
    TYPE_RDMSR,
    TYPE_WRMSR,
  } type;
  union {
    struct {
      CpuState *cpu;
      unsigned  cpuid_index;
    };
    struct {
      unsigned nr;
      unsigned reg;
      unsigned value;
    };
  };
  CpuMessage(Type _type, CpuState *_cpu) : type(_type), cpu(_cpu) { if (type == TYPE_CPUID) cpuid_index = cpu->eax; }
  CpuMessage(unsigned _nr, unsigned _reg, unsigned _value) : type(TYPE_CPUID_WRITE), nr(_nr), reg(_reg), value(_value) {}
};


class VCpu
{
  VCpu *_last;
public:
  DBus<CpuMessage> executor;

  VCpu *get_last() { return _last; }
  bool set_cpuid(unsigned nr, unsigned reg, unsigned value) {  CpuMessage msg(nr, reg, value); return executor.send(msg); }

  VCpu (VCpu *last) : _last(last) {}
};
