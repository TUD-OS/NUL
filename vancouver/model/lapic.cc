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

#ifndef REGBASE
class X2Apic : public LocalApic, public StaticReceiver<X2Apic>
{
  bool in_x2apic_mode;
  unsigned long long _msr;
#define REGBASE "lapic.cc"
#include "reg.h"

public:
  bool  receive(MessageMemRead &msg)
  {
    // XXX 64bit access
    // XXX distinguish CPUs
    if (!in_range(_msr & ~0xfff, msg.phys, 0x1000)     || msg.count != 4) return false;
    if (msg.phys & 0xf || (msg.phys & 0xfff) > 0x40*4 || !X2Apic_write((msg.phys >> 4) & 0x3f, *reinterpret_cast<unsigned *>(msg.ptr)))
      {}// XXX error indication
    return true;
  }


  bool  receive(MessageMemWrite &msg)
  {
    // XXX 64bit access
    // XXX distinguish CPUs
    if (!in_range(_msr & ~0xfff, msg.phys, 0x1000)     || msg.count != 4) return false;
    if (msg.phys & 0xf || (msg.phys & 0xfff) > 0x40*4 || !X2Apic_write((msg.phys >> 4) & 0x3f, *reinterpret_cast<unsigned *>(msg.ptr)))
      {}// XXX error indication
    return true;
  }


  bool  receive(MessageExecutor &msg) {

    // detect that we are on our CPU
    //XXX if (!msg.vcpu->apic != this) return false;
    bool res = false;
    switch (msg.cpu->head._pid) {
    case 31: // rdmsr
      if (msg.cpu->ecx == 0x1b) {
	msg.cpu->eax = _msr;
	msg.cpu->edx = _msr >> 32;
	res= true;
	break;
      }
      if (!in_x2apic_mode || !in_range(msg.cpu->ecx, 0x800, 64)) return false;
      if (msg.cpu->ecx != 0x831) {
	res = X2Apic_read(msg.cpu->ecx & 0x3f, msg.cpu->eax);
	msg.cpu->edx = (msg.cpu->ecx == 0x830) ? _ICR1 : 0;
      }
      break;
    case 32: // wrmsr
      if (msg.cpu->ecx == 0x1b) {
	msr = msg.cpu->eax & ~0x6ffu;
	msr |= static_cast<unsigned long long>(msg.cpu->edx & (1u << (Config::PHYS_ADDR_SIZE - 32)) - 1) << 32;
	res= true;
	break;
      }
      if (!in_x2apic_mode || !in_range(msg.cpu->ecx, 0x800, 64)) return false;
      if (msg.cpu->ecx != 0x831) {
	res = X2Apic_write(msg.cpu->ecx & 0x3f, msg.cpu->eax);
	if (msg.cpu->ecx == 0x830) X2Apic_write(0x31, msg.cpu->edx);
      }
      break;
    default:
      break;
    }
    if (res) {
      msg.cpu->eip += msg.cpu->inst_len;
      msg.cpu->head._pid = 0;
      return true;
    }
    return false;
  }


  bool  receive(MessageLegacy &msg) {
    if (msg.type == MessageLegacy::RESET) {
      _msr = 0xfee00800;
      if (!_ID) _msr |= 0x100;
      return true;
    }
    return false;
  }

  X2Apic(unsigned _initial_apic_id) : LocalApic(_initial_apic_id), in_x2apic_mode(false) {
    _ID = _initial_apic_id;
  }
};


PARAM(x2apic, {
    for (unsigned i=0; i < Config::NUM_VCPUS; i++) {
      X2Apic *dev = new X2Apic(i);
      mb.vcpustate(i)->apic = dev;
      mb.bus_legacy.add(dev,   &X2Apic::receive_static<MessageLegacy>);
      mb.bus_executor.add(dev, &X2Apic::receive_static<MessageExecutor>);
    }
  },
  "x2apic - provide an x2 APIC for every CPU");
#else
      // XXX calculate CCR+PPR
REGSET(X2Apic,
       REG_RW(_ID,            0x02,          0, 0)
       REG_RO(_VERSION,       0x03, 0x00050014)
       REG_RW(_ICR,           0x30,          0, ~0u)
       REG_RW(_ICR1,          0x31,          0, ~0u)
       REG_RW(_TIMER,         0x32, 0x00010000, 0x310ff)
       REG_RW(_TERM,          0x33, 0x00010000, 0x117ff)
       REG_RW(_PERF,          0x34, 0x00010000, 0x117ff)
       REG_RW(_LINT0,         0x35, 0x00010000, 0x1f7ff)
       REG_RW(_LINT1,         0x36, 0x00010000, 0x1f7ff)
       REG_RW(_ERROR,         0x37, 0x00010000, 0x110ff)
       REG_RW(_INITIAL_COUNT, 0x38,          0, ~0u)
       REG_RW(_CURRENT_COUNT, 0x39,          0, ~0u)
       REG_RW(_DIVIDE_CONFIG, 0x3e,          0, 0xb))
#endif
