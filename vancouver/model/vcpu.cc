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
#include "executor/bios.h"

#ifndef REGBASE
class VirtualCpu : public VCpu, public StaticReceiver<VirtualCpu>
{
#define REGBASE "../model/vcpu.cc"
#include "model/reg.h"

  unsigned long _hostop_id;
  Motherboard &_mb;

  volatile unsigned event;

  enum {
    MSR_TSC = 0x10,
    MSR_SYSENTER_CS = 0x174,
    MSR_SYSENTER_ESP,
    MSR_SYSENTER_EIP,
  };

  void handle_cpuid(CpuMessage &msg) {
    unsigned reg;
    if (msg.cpuid_index & 0x80000000u && msg.cpuid_index <= CPUID_EAX80)
      reg = (msg.cpuid_index << 4) | 0x80000000u;
    else {
      reg = msg.cpuid_index << 4;
      if (msg.cpuid_index > CPUID_EAX0) reg = CPUID_EAX0 << 4;
    }
    if (!CPUID_read(reg | 0, msg.cpu->eax)) msg.cpu->eax = 0;
    if (!CPUID_read(reg | 1, msg.cpu->ebx)) msg.cpu->ebx = 0;
    if (!CPUID_read(reg | 2, msg.cpu->ecx)) msg.cpu->ecx = 0;
    if (!CPUID_read(reg | 3, msg.cpu->edx)) msg.cpu->edx = 0;
  }


  void handle_rdtsc(CpuMessage &msg) {
    msg.cpu->edx_eax(msg.cpu->tsc_off + Cpu::rdtsc());
    msg.mtr_out |= MTD_GPR_ACDB;
  }


  void handle_rdmsr(CpuMessage &msg) {
    switch (msg.cpu->ecx) {
    case MSR_TSC:
      handle_rdtsc(msg);
      break;
    case MSR_SYSENTER_CS:
    case MSR_SYSENTER_ESP:
    case MSR_SYSENTER_EIP:
      assert(msg.mtr_in & MTD_SYSENTER);
      msg.cpu->edx_eax((&msg.cpu->sysenter_cs)[msg.cpu->ecx - MSR_SYSENTER_CS]);
      break;
    case 0x8b: // microcode
      // MTRRs
    case 0x250:
    case 0x258:
    case 0x259:
    case 0x268 ... 0x26f:
      msg.cpu->edx_eax(0);
      break;
    default:
      Logging::printf("unsupported rdmsr %x at %x",  msg.cpu->ecx, msg.cpu->eip);
      msg.cpu->GP0();
    }
    msg.mtr_out |= MTD_GPR_ACDB;
  }


  void handle_wrmsr(CpuMessage &msg) {
    CpuState *cpu = msg.cpu;
    switch (cpu->ecx)
      {
      case MSR_TSC:
	cpu->tsc_off = -Cpu::rdtsc() + cpu->edx_eax();
	Logging::printf("reset RDTSC to %llx at %x value %llx\n", cpu->tsc_off, cpu->eip, cpu->edx_eax());
	break;
      case MSR_SYSENTER_CS:
      case MSR_SYSENTER_ESP:
      case MSR_SYSENTER_EIP:
	(&cpu->sysenter_cs)[cpu->ecx - MSR_SYSENTER_CS] = cpu->edx_eax();
	msg.mtr_out |= MTD_SYSENTER;
	break;
      default:
	Logging::printf("unsupported wrmsr %x <-(%x:%x) at %x",  cpu->ecx, cpu->edx, cpu->eax, cpu->eip);
	cpu->GP0();
      }
  }


  void handle_cpu_init(CpuMessage &msg, bool reset) {
    CpuState *cpu = msg.cpu;
    memset(cpu->msg, 0, sizeof(cpu->msg));
    cpu->efl      = 2;
    cpu->eip      = 0xfff0;
    cpu->cr0      = 0x10;
    cpu->cs.ar    = 0x9b;
    cpu->cs.limit = 0xffff;
    cpu->cs.base  = 0xffff0000;
    cpu->cs.sel   = 0xf000;
    cpu->ss.ar    = 0x93;
    cpu->edx      = 0x600;
    cpu->ds.ar = cpu->es.ar = cpu->fs.ar = cpu->gs.ar = cpu->ss.ar;
    cpu->ld.ar    = 0x1000;
    cpu->tr.ar    = 0x8b;
    cpu->ss.limit = cpu->ds.limit = cpu->es.limit = cpu->fs.limit = cpu->gs.limit = cpu->cs.limit;
    cpu->tr.limit = cpu->ld.limit = cpu->gd.limit = cpu->id.limit = 0xffff;
    /*cpu->dr6      = 0xffff0ff0;*/
    // cpu->_dr = {0, 0, 0, 0};
    cpu->dr7      = 0x400;
    msg.mtr_out  |= MTD_ALL;
    if (reset) {
      cpu->tsc_off = -Cpu::rdtsc();
      // XXX floating point
      // XXX MXCSR
      // XXX MTRR
      // XXX APIC
      // XXX PERF
    }
  }


  bool can_inject(CpuState *cpu) {  return !(cpu->intr_state & 0x3) && cpu->efl & 0x200; }
  bool inject_interrupt(CpuState *cpu, unsigned vec) {
    // spurious vector?
    if (vec == ~0u) return true;

    // already injection in progress?
    if (~cpu->inj_info & 0x80000000 && can_inject(cpu)) {
      cpu->inj_info = vec | 0x80000000;
      return true;
    }
    return false;
  }

  void handle_irq(CpuMessage &msg) {

    CpuState *cpu = msg.cpu;
    assert(msg.mtr_in & MTD_STATE);
    assert(msg.mtr_in & MTD_INJ);
    assert(msg.mtr_in & MTD_RFLAGS);

    unsigned or_mask  = 0;
    unsigned and_mask = 0;
    unsigned old_event = event;
    do {
      if (old_event & EVENT_RESET) {
	handle_cpu_init(msg, true);
	and_mask |= EVENT_RESET;

	// are we an AP and should go to the wait-for-sipi state?
	if (is_ap()) {
	  cpu->actv_state = 3;
	  or_mask |= STATE_WFS;
	}
	break;
      }


      // SIPI pending?
      if (old_event & EVENT_SIPI) {
	cpu->eip          = 0;
	cpu->cs.base      = (old_event & 0xff00) << 4;
	cpu->cs.sel       =  old_event & 0xff00;
	cpu->actv_state   = 0;
	and_mask |= 0xff00 | EVENT_SIPI;
      }

      // do block everything until we got an SIPI
      if (cpu->actv_state == 3) break;

      // INIT
      if (old_event & EVENT_INIT) {
	handle_cpu_init(msg, false);
	cpu->actv_state = 3;
	and_mask |= EVENT_INIT;
	or_mask  |= STATE_WFS;
	break;
      }

      // SMI
      if (old_event & EVENT_SMI && ~cpu->intr_state & 4) {
	Logging::printf("SMI received\n");
	and_mask |= EVENT_SMI;
	cpu->actv_state = 0;
      }

      // NMI
      if (old_event & EVENT_NMI && ~cpu->intr_state & 8 && ~cpu->inj_info & 0x80000000 && !(cpu->intr_state & 3)) {
	cpu->inj_info = 0x80000200;
	cpu->actv_state = 0;
	and_mask |= EVENT_NMI;
	break;
      }

      // interrupts are blocked in shutdown
      if (cpu->actv_state == 2) break;

      // ExtINT
      if (old_event & EVENT_EXTINT) {
	MessageLegacy msg2(MessageLegacy::INTA, ~0u);
	_mb.bus_legacy.send(msg2);
	if (inject_interrupt(cpu, msg2.value))
	  and_mask |= EVENT_EXTINT;
	cpu->actv_state = 0;
	break;
      }

      // FIXED APIC interrupt
      if (old_event & EVENT_FIXED) {
	// XXX go to the APIC
	and_mask |= EVENT_FIXED;
	cpu->actv_state = 0;
	break;
      }
    } while (0);

    /**
     * The order is important here, as we have to delete the SIPI
     * first and then enable the wait-for-sipi bit.
     */
    Cpu::atomic_and<volatile unsigned>(&event, ~and_mask);
    Cpu::atomic_or <volatile unsigned>(&event,   or_mask);
    old_event = event;

    // recalculate the IRQ windows
    cpu->inj_info &= ~(INJ_IRQWIN | INJ_NMIWIN);
    if (old_event & (EVENT_EXTINT | EVENT_FIXED))  cpu->inj_info |= INJ_IRQWIN;
    if (old_event & EVENT_NMI)                     cpu->inj_info |= INJ_NMIWIN;
  }


  void handle_ioin(CpuMessage &msg) {
    MessageIOIn msg2(MessageIOIn::Type(msg.io_order), msg.port);
    bool res = _mb.bus_ioin.send(msg2);

    Cpu::move(msg.dst, &msg2.value, msg.io_order);
    msg.mtr_out |= MTD_GPR_ACDB;

    static unsigned char debugioin[8192];
    if (!res && ~debugioin[msg.port >> 3] & (1 << (msg.port & 7))) {
      debugioin[msg.port >> 3] |= 1 << (msg.port & 7);
      Logging::panic("could not read from ioport %x eip %x cs %x-%x\n", msg.port, msg.cpu->eip, msg.cpu->cs.base, msg.cpu->cs.ar);
    }
  }


  void handle_ioout(CpuMessage &msg) {
    MessageIOOut msg2(MessageIOOut::Type(msg.io_order), msg.port, 0);
    Cpu::move(&msg2.value, msg.dst, msg.io_order);
    bool res = _mb.bus_ioout.send(msg2);

    static unsigned char debugioout[8192];
    if (!res && ~debugioout[msg.port >> 3] & (1 << (msg.port & 7))) {
      debugioout[msg.port >> 3] |= 1 << (msg.port & 7);
      Logging::printf("could not write to ioport %x eip %x\n", msg.port, msg.cpu->eip);
    }
  }


  void got_event(unsigned value) {
    /**
     * We received an asynchronous event. As code runs in many
     * threads, state updates have to be atomic!
     */
    unsigned old_value;
    unsigned new_value;
    do {
      old_value = event;
      new_value = old_value | (value & EVENT_MASK);
      if (old_value == new_value) return;
      if ((value & EVENT_MASK) == EVENT_SIPI) {
	if (~old_value & STATE_WFS) return;
	new_value = (new_value & ~(STATE_WFS & 0xff00)) | value;
      }
    } while (Cpu::cmpxchg(&event, old_value, new_value) != old_value);

    Cpu::atomic_or<volatile unsigned>(&event, STATE_WAKEUP);
    MessageHostOp msg(MessageHostOp::OP_VCPU_RELEASE, _hostop_id, event & STATE_BLOCK);
    _mb.bus_hostop.send(msg);
  }

public:

  bool receive(MessageLegacy &msg) {
    if (msg.type == MessageLegacy::RESET) {
      got_event(EVENT_RESET);
      return true;
    }
    // only the BSP receives legacy signals
    if (is_ap()) return false;
    if (msg.type == MessageLegacy::EXTINT)
      got_event(EVENT_EXTINT);
    else if (msg.type == MessageLegacy::NMI)
      got_event(EVENT_NMI);
    else if (msg.type == MessageLegacy::INIT)
      got_event(EVENT_INIT);
    else return false;
    return true;
  }



  bool receive(CpuMessage &msg) {
    switch (msg.type) {
    case CpuMessage::TYPE_CPUID:
      handle_cpuid(msg);
      break;
    case CpuMessage::TYPE_CPUID_WRITE:
      {
	unsigned reg = (msg.nr << 4) | msg.reg | msg.nr & 0x80000000;
	unsigned old;
	return CPUID_read(reg, old) && CPUID_write(reg, (old & msg.mask) | msg.value);
      };
    case CpuMessage::TYPE_RDTSC:
      handle_rdtsc(msg);
      break;
    case CpuMessage::TYPE_RDMSR:
      handle_rdmsr(msg);
      break;
    case CpuMessage::TYPE_WRMSR:
      handle_wrmsr(msg);
      break;
    case CpuMessage::TYPE_IOIN:
      handle_ioin(msg);
      break;
    case CpuMessage::TYPE_IOOUT:
      handle_ioout(msg);
      break;
    case CpuMessage::TYPE_TRIPLE:
      assert(!msg.cpu->actv_state);
      msg.cpu->actv_state = 2;
      break;
    case CpuMessage::TYPE_INIT:
      got_event(EVENT_INIT);
      break;
    case CpuMessage::TYPE_HLT:
      assert(!msg.cpu->actv_state);
      msg.cpu->actv_state = 1;
      break;
    case CpuMessage::TYPE_CHECK_IRQ:
      // we handle it later on
      break;
    case CpuMessage::TYPE_SINGLE_STEP:
    default:
      return false;
    }

    // handle IRQ injection
    for (handle_irq(msg); msg.cpu->actv_state & 0x3; handle_irq(msg)) {
      MessageHostOp msg2(MessageHostOp::OP_VCPU_BLOCK, _hostop_id);
      Cpu::atomic_or<volatile unsigned>(&event, STATE_BLOCK);
      if (~event & STATE_WAKEUP) _mb.bus_hostop.send(msg2);
      Cpu::atomic_and<volatile unsigned>(&event, ~(STATE_BLOCK | STATE_WAKEUP));
    }
    return true;
  }

  VirtualCpu(VCpu *_last, Motherboard &mb) : VCpu(_last), _mb(mb) {
    MessageHostOp msg(this);
    if (!mb.bus_hostop.send(msg)) Logging::panic("could not create VCpu backend.");
    _hostop_id = msg.value;
    mb.bus_legacy.add(this, &VirtualCpu::receive_static<MessageLegacy>);
    CPUID_reset();
 }
};

PARAM(vcpu,
      VirtualCpu *dev = new VirtualCpu(mb.last_vcpu, mb);
      dev->executor.add(dev, &VirtualCpu::receive_static<CpuMessage>);
      mb.last_vcpu = dev;
      ,
      "vcpu - create a new VCPU");

#else
REGSET(CPUID,
       REG_RW(CPUID_EAX0,  0x00, 2, ~0u)
       REG_RW(CPUID_EBX0,  0x01, 0, ~0u)
       REG_RW(CPUID_ECX0,  0x02, 0, ~0u)
       REG_RW(CPUID_EDX0,  0x03, 0, ~0u)
       REG_RW(CPUID_EAX1,  0x10, 0x673, ~0u)
       REG_RW(CPUID_EBX1,  0x11, 0, ~0u)
       REG_RW(CPUID_ECX1,  0x12, 0, ~0u)
       REG_RW(CPUID_EDX1,  0x13, 0, ~0u)
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
