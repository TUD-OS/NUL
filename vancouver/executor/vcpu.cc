/**
 * Virtual CPU.
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

#include "nul/motherboard.h"
#include "nul/vcpu.h"

#ifndef REGBASE
class VirtualCpu : public VCpu, public StaticReceiver<VirtualCpu>
{
#define REGBASE "../executor/vcpu.cc"
#include "model/reg.h"

  unsigned get_cpuid_reg(unsigned cpuid_index)
  {
    unsigned index;
    if (cpuid_index & 0x80000000u && cpuid_index <= CPUID_EAX80)
      index = (cpuid_index << 4) | 0x80000000u;
    else {
      index = cpuid_index << 4;
      if (cpuid_index > CPUID_EAX0) index = CPUID_EAX0 << 4;
    }
    return index;
  }
public:
  bool receive(CpuMessage &msg) {
    switch (msg.type) {
    case CpuMessage::TYPE_CPUID:
      {
	unsigned reg = get_cpuid_reg(msg.cpuid_index);
	if (!CPUID_read(reg+0, msg.cpu->eax)) msg.cpu->eax = 0;
	if (!CPUID_read(reg+1, msg.cpu->ebx)) msg.cpu->ebx = 0;
	if (!CPUID_read(reg+2, msg.cpu->ecx)) msg.cpu->ecx = 0;
	if (!CPUID_read(reg+3, msg.cpu->edx)) msg.cpu->edx = 0;
      }
      break;
    case CpuMessage::TYPE_CPUID_WRITE:
      CPUID_write((msg.nr << 4) | msg.reg | msg.nr & 0x80000000, msg.value);
      break;
    case CpuMessage::TYPE_RDMSR:
    case CpuMessage::TYPE_WRMSR:
    default: return false;
    }
    return true;
  }

  VirtualCpu(VCpu *_last) : VCpu(_last) {  CPUID_reset();  }
};

PARAM(vcpu,
      VirtualCpu *dev = new VirtualCpu(mb.last_vcpu);
      dev->executor.add(dev, &VirtualCpu::receive_static, CpuMessage::TYPE_CPUID);
      mb.last_vcpu = dev;
      MessageHostOp msg(dev);
      if (!mb.bus_hostop.send(msg)) Logging::panic("could not create VCpu backend.");

      ,
      "vcpu - create a new VCPU");

#else
REGSET(CPUID,
       REG_RW(CPUID_EAX0,  0x00, 2, ~0u)
       REG_RW(CPUID_EBX0,  0x01, 0, ~0u)
       REG_RW(CPUID_ECX0,  0x02, 0, ~0u)
       REG_RW(CPUID_EDX0,  0x03, 0, ~0u)
       REG_RW(CPUID_EAX1,  0x10, 0x673, ~0u)
       REG_RW(CPUID_EDX1,  0x13, 0x0182a9ff, ~0u)
       REG_RW(CPUID_EAX80, 0x80000000, 0x80000004, ~0u)
       REG_RW(CPUID_ECX81, 0x80000012, 0x100000, ~0u)
       REG_RW(CPUID_EAX82, 0x80000020, 0, ~0u)
       REG_RW(CPUID_EBX82, 0x80000021, 0, ~0u)
       REG_RW(CPUID_ECX82, 0x80000022, 0, ~0u)
       REG_RW(CPUID_EDX82, 0x80000023, 0, ~0u)
       REG_RW(CPUID_EAX83, 0x80000030, 0, ~0u)
       REG_RW(CPUID_EBX83, 0x80000031, 0, ~0u)
       REG_RW(CPUID_ECX83, 0x80000032, 0, ~0u)
       REG_RW(CPUID_EDX83, 0x80000033, 0, ~0u)
       REG_RW(CPUID_EAX84, 0x80000040, 0, ~0u)
       REG_RW(CPUID_EBX84, 0x80000041, 0, ~0u)
       REG_RW(CPUID_ECX84, 0x80000042, 0, ~0u)
       REG_RW(CPUID_EDX84, 0x80000043, 0, ~0u))
#endif
