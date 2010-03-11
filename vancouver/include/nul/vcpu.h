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
    TYPE_RDMSR,
    TYPE_WRMSR,
  } type;
  Utcb    &cpu;
  union {
    unsigned cpuid_index;
  };
  CpuMessage(Type _type, Utcb &_cpu) : type(_type), cpu(_cpu) { if (type == TYPE_CPUID) cpuid_index = cpu.eax; }
};


class VCpu
{
public:
  DBus<CpuMessage> executor;
};
