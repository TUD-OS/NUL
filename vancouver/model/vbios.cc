/** @file
 * VCPU to VBios bridge.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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
#include "executor/bios.h"

class VBios : public DiscoveryHelper<VBios>, public StaticReceiver<VBios>
{
public:
  Motherboard &_mb;
private:
  VCpu *_vcpu;
  unsigned char _resetvector[16];
  enum {  BIOS_BASE = 0xf0000 };

public:
  bool receive(CpuMessage &msg) {
    if (msg.type != CpuMessage::TYPE_SINGLE_STEP) return false;

    CpuState *cpu = msg.cpu;
    if (cpu->pm() && !cpu->v86()
	|| !in_range(cpu->cs.base + cpu->eip, BIOS_BASE, BiosCommon::MAX_VECTOR)
	|| cpu->inj_info & 0x80000000) return false;

    COUNTER_INC("VB");
    unsigned irq =  (cpu->cs.base + cpu->eip) - BIOS_BASE;

    /**
     * Normally we jump to the last instruction in the 16-byte reset
     * area where we provide an IRET instruction to the instruction
     * emulator.
     */
    cpu->cs.sel  = BIOS_BASE >> 4;
    cpu->cs.base = BIOS_BASE;
    cpu->eip = 0xffff;
    msg.mtr_out |= MTD_CS_SS | MTD_RIP_LEN | MTD_RFLAGS;

    MessageBios msg1(_vcpu, cpu, irq);
    if (!_mb.bus_bios.send(msg1, irq != BiosCommon::RESET_VECTOR)) return false;

    // we have to propagate the flags to the user stack!
    unsigned flags;
    MessageMem msg2(true, cpu->ss.base + cpu->esp + 4, &flags);
    _vcpu->mem.send(msg2);
    flags = flags & ~0xffffu | cpu->efl & 0xffffu | flags & 0x200;
    msg2.read = false;
    _vcpu->mem.send(msg2);

    // we can be sure that we are not blocking irqs
    assert(!cpu->actv_state);
    // XXX Triggers in QEMU. QEMU bug?
    //assert(!(cpu->intr_state & 3));
    msg.mtr_out |= msg1.mtr_out;
    return true;

  }

  /**
   * The memory read routine for the last 16byte below 4G and below 1M.
   */
  bool receive(MessageMem &msg)
  {
    if (!msg.read || !in_range(msg.phys, 0xfffffff0, 0x10) && !in_range(msg.phys, BIOS_BASE + 0xfff0, 0x10))  return false;
    *msg.ptr = *reinterpret_cast<unsigned *>(_resetvector + (msg.phys & 0xc));
    return true;
  }


  bool  receive(MessageDiscovery &msg) {
    if (msg.type != MessageDiscovery::DISCOVERY) return false;

    // initialize realmode idt
    unsigned value = (BIOS_BASE >> 4) << 16;
    for (unsigned i=0; i < 256; i++) {
      // XXX init only whats needed and done on compatible BIOSes
      if (i != 0x43)
	discovery_write_dw("realmode idt", i*4, value, 4);
      value++;
    }
    return true;
  }



  VBios(Motherboard &mb, VCpu *vcpu) : _mb(mb), _vcpu(vcpu) {

    // initialize the reset vector with noops
    memset(_resetvector, 0x90, sizeof(_resetvector));
    // realmode longjump to reset code
    _resetvector[0x0] = 0xea;
    _resetvector[0x1] = BiosCommon::RESET_VECTOR & 0xff;
    _resetvector[0x2] = BiosCommon::RESET_VECTOR >> 8;
    _resetvector[0x3] = (BIOS_BASE >> 4) & 0xff;
    _resetvector[0x4] = BIOS_BASE >> 12;

    // the hlt for do_hlt()
    _resetvector[0xe] = 0xf4;

    // the iret that is the default operation
    _resetvector[0xf] = 0xcf;
    _vcpu->executor.add(this,   VBios::receive_static<CpuMessage>);
    _vcpu->mem.add(this,        VBios::receive_static<MessageMem>);
    _mb.bus_discovery.add(this, VBios::receive_static<MessageDiscovery>);

  }

};

PARAM_HANDLER(vbios,
	      "vbios - create a bridge between VCPU and the BIOS bus.")
{
  if (!mb.last_vcpu) Logging::panic("no VCPU for this VBIOS");
  new VBios(mb, mb.last_vcpu);
}

