/**
 * Local APIC model.
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

#include "model/lapic.h"
#include "nul/motherboard.h"


class X2Apic : public LocalApic, public StaticReceiver<X2Apic>
{
  bool x2apic_mode;
  void edx_eax(CpuState *cpu, unsigned long long value)
  {
    cpu->eax = value;
    cpu->edx = value >> 32;
  };
  unsigned long long edx_eax(CpuState *cpu) {  return static_cast<unsigned long long>(cpu->edx) << 32 | cpu->eax; };

public:
  bool  receive(MessageMemRead &msg)
  {
    // XXX distinguish CPUs
    if (!in_range(msr & ~0xfff, msg.phys, 0x1000) || msg.count != 4) return false;
    return true;
  }


  bool  receive(MessageMemWrite &msg)
  {
    // XXX distinguish CPUs
    if (!in_range(msr & ~0xfff, msg.phys, 0x1000)) return false;
    return true;
  }


  bool  receive(MessageExecutor &msg) {

    if (msg.vcpu->apic != this) return false;

    switch (msg.cpu->head.pid) {
    case 31: // rdmsr
      switch (msg.cpu->ecx) {
      case 0x1b: // APIC base MSR
	edx_eax(msg.cpu, msr); break;
      case 0x802: // initial APIC id
	edx_eax(msg.cpu, initial_apic_id); break;
      default: return false;
      }
    case 32: // wrmsr
      switch (msg.cpu->ecx) {
      case 0x1b: // APIC base MSR
	msr = edx_eax(msg.cpu) & ((1ull << Config::PHYS_ADDR_SIZE) - 1) & ~0x6ffull;
	break;
      }
    default: return false;
    }

    msg.cpu->eip += msg.cpu->inst_len;
    msg.cpu->head.pid = 0;
    return true;
  }


  bool  receive(MessageLegacy &msg) {
    if (msg.type == MessageLegacy::RESET) {
      msr = initial_apic_id ? 0xfee00800 :  0xfee00900;
      return true;
    }
    return false;
  }


  X2Apic(unsigned _initial_apic_id) : LocalApic(_initial_apic_id), x2apic_mode(false) {}
};

PARAM(x2apic, {
    for (unsigned i=0; i < Config::NUM_VCPUS; i++) {
      X2Apic *dev = new X2Apic(i);
      mb.vcpustate(i)->apic = dev;
      mb.bus_legacy.add(dev, &X2Apic::receive_static<MessageLegacy>);
      mb.bus_executor.add(dev, &X2Apic::receive_static<MessageExecutor>);
    }
  },
  "x2apic - provide an x2 APIC for every CPU");
