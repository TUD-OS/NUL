/**
 * IRQ executor.
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
#include "vmm/motherboard.h"

/**
 * Handle IRQ injection.
 *
 * State: unstable
 */
class IrqExecutor : public StaticReceiver<IrqExecutor>
{
  Motherboard &_mb;
  bool can_inject(CpuState *cpu) {  return !(cpu->intr_state & 0x3) && cpu->efl & 0x200; }
  void inject_extint(CpuState *cpu, unsigned vec) {
    cpu->actv_state &= ~1;
    cpu->inj_info = vec | 0x80000000;
  }
  const char *debug_getname() { return "IrqExecutor"; };
 public:
  bool  receive(MessageExecutor &msg)
  {
    if (msg.cpu->head.pid != 1 && msg.cpu->head.pid != 7)  return false;

    CpuState *cpu = msg.cpu;
    assert_mtr(MTD_STATE | MTD_INJ | MTD_RFLAGS);

    bool res = false;
    if (msg.vcpu->hazard & VirtualCpuState::HAZARD_IRQ)
      {
	COUNTER_INC("lint0");
	if (((~msg.cpu->head.mtr.untyped() & MTD_INJ) || ~msg.cpu->inj_info & 0x80000000) && can_inject(msg.cpu))
	  {
	    Cpu::atomic_and<volatile unsigned>(&msg.vcpu->hazard, ~VirtualCpuState::HAZARD_IRQ);
	    if (msg.vcpu->lastmsi && msg.vcpu->lastmsi != 0xff)
	      {
		Logging::printf("inject MSI %x\n", msg.vcpu->lastmsi);
		COUNTER_INC("injmsi");
		inject_extint(msg.cpu, msg.vcpu->lastmsi & 0xff);
		msg.vcpu->lastmsi = 0;
	      }
	    else {
	      MessageApic msg2(0);
	      if (_mb.bus_apic.send(msg2))
		{
		  inject_extint(msg.cpu, msg2.vector);
		  COUNTER_INC("inj");
		}
	      else
		Logging::panic("spurious IRQ?");
	    }
	    res = true;
	  }
	else
	  msg.cpu->inj_info |=  INJ_IRQWIN;
      }
    else
      msg.cpu->inj_info &= ~INJ_IRQWIN;

    // get back to the inst emulator???
    msg.cpu->head.pid = MessageExecutor::DO_SINGLESTEP;
    return true;
  }

 IrqExecutor(Motherboard &mb) : _mb(mb) {}
};

PARAM(irq,
      {
	Device *dev = new IrqExecutor(mb);
	mb.bus_executor.add(dev,  &IrqExecutor::receive_static, 1);
	mb.bus_executor.add(dev,  &IrqExecutor::receive_static, 7);
      },
      "irq - create an executor that handles irq injection.");

