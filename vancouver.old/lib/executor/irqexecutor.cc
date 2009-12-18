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

using namespace Nova;

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
    assert_mtr(MTD_STA | MTD_INJ | MTD_EFL);

    bool res = false;
    if (msg.vcpu->hazard & VirtualCpuState::HAZARD_IRQ)
      {
	COUNTER_INC("lint0");
	//if (msg.vcpu->instcache->debug) Logging::printf("check IRQ %lx lint0 %x rip %x %x %x %x mtr %x\n", -1UL, msg.vcpu->hazard, msg.cpu->eip, msg.cpu->inj_info, msg.cpu->actv_state, msg.cpu->efl, msg.cpu->head.mtr.untyped());
	//Logging::printf("%s() mtr %x rip %x ilen %x cr0 %x efl %x\n", __func__, msg.cpu->head.mtr, msg.cpu->eip, msg.cpu->inst_len, msg.cpu->cr0, msg.cpu->efl);
	if (((~mtd_untyped(msg.cpu->head.mtr) & MTD_INJ) || ~msg.cpu->inj_info & 0x80000000) && can_inject(msg.cpu))
	  {
	    msg.vcpu->hazard &= ~VirtualCpuState::HAZARD_IRQ;
	    MessageApic msg2(0);
	    if (_mb.bus_apic.send(msg2))
	      {
		inject_extint(msg.cpu, msg2.vector);
		COUNTER_INC("inj"); 
		//if ((vec & 0xf) == 1 ) Logging::printf("check IRQ %lx lint0 %x rip %lx %lx\n", vec, _lint0, s->rip(), s->rflags());
		//if ((msg2.vector & 0xf) != 0) Logging::printf("check IRQ %x rip %x %x %x %x mtr %x\n", msg2.vector, msg.cpu->eip, msg.cpu->inj_info, msg.cpu->actv_state, msg.cpu->efl, msg.cpu->head.mtr.untyped());
	      }
	    else
	      Logging::panic("spurious IRQ?");
	    res = true;
	  }
	else
	  msg.cpu->inj_info |=  INJ_IRQWIN;
      }
    else
      msg.cpu->inj_info &= ~INJ_IRQWIN;

    // get back to the inst emulator???
    msg.cpu->head.pid = 33;
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

