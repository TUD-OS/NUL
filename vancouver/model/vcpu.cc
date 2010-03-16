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


  void handle_init(CpuMessage &msg) {
    CpuState *cpu = msg.cpu;
    memset(cpu->msg, 0, sizeof(cpu->msg));
    cpu->eip      = 0xfff0;
    cpu->cr0      = 0x10;
    cpu->cs.ar    = 0x9b;
    cpu->cs.limit = 0xffff;
    cpu->cs.base  = 0xffff0000;
    cpu->ss.ar    = 0x93;
    cpu->edx      = 0x600;
    cpu->ds.ar = cpu->es.ar = cpu->fs.ar = cpu->gs.ar = cpu->ss.ar;
    cpu->ld.ar    = 0x1000;
    cpu->tr.ar    = 0x8b;
    cpu->ss.limit = cpu->ds.limit = cpu->es.limit = cpu->fs.limit = cpu->gs.limit = cpu->cs.limit;
    cpu->tr.limit = cpu->ld.limit = cpu->gd.limit = cpu->id.limit = 0xffff;
    /*cpu->dr6      = 0xffff0ff0;*/
    cpu->dr7      = 0x400;
    // goto singlestep instruction?
    cpu->efl      = 0;
    msg.mtr_out   = MTD_ALL;
  }


  bool can_inject(CpuState *cpu) {  return !(cpu->intr_state & 0x3) && cpu->efl & 0x200; }
  void inject_extint(CpuState *cpu, unsigned vec) {
    cpu->actv_state &= ~1;
    cpu->inj_info = vec | 0x80000000;
  }

  void handle_irq(CpuMessage &msg) {

    CpuState *cpu = msg.cpu;
    assert(msg.mtr_in & MTD_STATE);
    assert(msg.mtr_in & MTD_INJ);
    assert(msg.mtr_in & MTD_RFLAGS);

    bool res = false;
    if (hazard & HAZARD_IRQ)
      {
	COUNTER_INC("lint0");
	if ((~cpu->inj_info & 0x80000000) && can_inject(cpu))
	  {
	    Cpu::atomic_and<volatile unsigned>(&hazard, ~HAZARD_IRQ);
	    if (lastmsi && lastmsi != 0xff)
	      {
		Logging::printf("inject MSI %x\n", lastmsi);
		COUNTER_INC("injmsi");
		inject_extint(cpu, lastmsi & 0xff);
		lastmsi = 0;
	      }
	    else {
	      MessageApic msg2(0);
	      if (_mb.bus_apic.send(msg2))
		{
		  inject_extint(cpu, msg2.vector);
		  COUNTER_INC("inj");
		}
	      else
		Logging::panic("spurious IRQ?");
	    }
	    res = true;
	  }
	else
	  cpu->inj_info |=  INJ_IRQWIN;
      }
    else
      cpu->inj_info &= ~INJ_IRQWIN;
  }


  void handle_hlt(CpuMessage &msg) {
    msg.cpu->actv_state &= ~1u;
    MessageHostOp msg2(MessageHostOp::OP_VCPU_BLOCK, _hostop_id);
    Cpu::atomic_or<volatile unsigned>(&hazard, HAZARD_INHLT);
    if (~hazard & HAZARD_IRQ) _mb.bus_hostop.send(msg2);
    Cpu::atomic_and<volatile unsigned>(&hazard, ~HAZARD_INHLT);
  }

  void skip_instruction(CpuMessage &msg)
  {
    assert(msg.mtr_in & MTD_RIP_LEN);
    assert(msg.mtr_in & MTD_STATE);
    msg.cpu->eip += msg.cpu->inst_len;
    /**
     * Cancel sti and mov-ss blocking as we emulated an instruction.
     */
    msg.cpu->intr_state &= ~3;
  }


public:

  bool receive(CpuMessage &msg) {
    bool skip = false;
    switch (msg.type) {
    case CpuMessage::TYPE_CPUID:
      handle_cpuid(msg);
      skip = true;
      break;
    case CpuMessage::TYPE_CPUID_WRITE:
      {
	unsigned reg = (msg.nr << 4) | msg.reg | msg.nr & 0x80000000;
	unsigned old;
	return CPUID_read(reg, old) && CPUID_write(reg, (old & msg.mask) | msg.value);
      };
    case CpuMessage::TYPE_RDTSC:
      handle_rdtsc(msg);
      skip = true;
      break;
    case CpuMessage::TYPE_RDMSR:
      handle_rdmsr(msg);
      skip = true;
      break;
    case CpuMessage::TYPE_WRMSR:
      handle_wrmsr(msg);
      skip = true;
      break;
    case CpuMessage::TYPE_TRIPLE:
      {
	// XXX that will generate an INIT signal if not blocked
	MessageLegacy msg1(MessageLegacy::RESET, 0);
	_mb.bus_legacy.send_fifo(msg1);
      }
      break;
    case CpuMessage::TYPE_INIT:
      handle_init(msg);
      break;
    case CpuMessage::TYPE_HLT:
      handle_hlt(msg);
      skip = true;
      break;
    case CpuMessage::TYPE_CHECK_IRQ:
      // we handle it later on
      break;
    case CpuMessage::TYPE_WAKEUP:
      {
	MessageHostOp msg(MessageHostOp::OP_VCPU_RELEASE, _hostop_id, hazard & HAZARD_INHLT);
	_mb.bus_hostop.send(msg);
      }
      break;
    case CpuMessage::TYPE_SINGLE_STEP:
    default:
      return false;
    }

    if (skip) skip_instruction(msg);

    // handle IRQ injection
    handle_irq(msg);
    return true;
  }

  VirtualCpu(VCpu *_last, Motherboard &mb) : VCpu(_last), _mb(mb) {
    MessageHostOp msg(this);
    if (!mb.bus_hostop.send(msg)) Logging::panic("could not create VCpu backend.");
    _hostop_id = msg.value;

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
