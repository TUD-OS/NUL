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

  volatile unsigned _event;
  volatile unsigned _sipi;

  void dprintf(const char *format, ...) {
    Logging::printf("[%2d] ", CPUID_EDXb);
    va_list ap;
    va_start(ap, format);
    Logging::vprintf(format, ap);
    va_end(ap);

  }

  void GP0(CpuMessage &msg) {
    msg.cpu->inj_info = 0x80000b0d;
    msg.cpu->inj_error = 0;
    msg.mtr_out |= MTD_INJ;
  }


  bool handle_cpuid(CpuMessage &msg) {
    bool res = true;
    unsigned reg;
    if (msg.cpuid_index & 0x80000000u && msg.cpuid_index <= CPUID_EAX80)
      reg = (msg.cpuid_index << 4) | 0x80000000u;
    else {
      reg = msg.cpuid_index << 4;
      if (msg.cpuid_index > CPUID_EAX0) {
	reg = CPUID_EAX0 << 4;
	res = false;
      }
    }
    if (!CPUID_read(reg | 0, msg.cpu->eax)) msg.cpu->eax = 0;
    if (!CPUID_read(reg | 1, msg.cpu->ebx)) msg.cpu->ebx = 0;
    if (!CPUID_read(reg | 2, msg.cpu->ecx)) msg.cpu->ecx = 0;
    if (!CPUID_read(reg | 3, msg.cpu->edx)) msg.cpu->edx = 0;
    msg.mtr_out |= MTD_GPR_ACDB;
    return res;
  }


  void handle_rdtsc(CpuMessage &msg) {
    assert(msg.mtr_in & MTD_TSC);
    msg.cpu->edx_eax(msg.cpu->tsc_off + Cpu::rdtsc());
    msg.mtr_out |= MTD_GPR_ACDB;
  }


  void handle_rdmsr(CpuMessage &msg) {
    switch (msg.cpu->ecx) {
    case 0x10:
      handle_rdtsc(msg);
      break;
    case 0x174 ... 0x176:
      assert(msg.mtr_in & MTD_SYSENTER);
      msg.cpu->edx_eax((&msg.cpu->sysenter_cs)[msg.cpu->ecx - 0x174]);
      break;
    case 0x8b: // microcode
      // MTRRs
    case 0xfe:
    case 0x250:
    case 0x258:
    case 0x259:
    case 0x268 ... 0x26f:
    case 0x2ff:
      msg.cpu->edx_eax(0);
      break;
    default:
      dprintf("unsupported rdmsr %x at %x\n",  msg.cpu->ecx, msg.cpu->eip);
      GP0(msg);
    }
    msg.mtr_out |= MTD_GPR_ACDB;
  }


  void handle_wrmsr(CpuMessage &msg) {
    CpuState *cpu = msg.cpu;
    switch (cpu->ecx)
      {
      case 0x10:
	cpu->tsc_off = -Cpu::rdtsc() + cpu->edx_eax();
	msg.mtr_out |= MTD_TSC;
	//dprintf("reset RDTSC to %llx at %x value %llx\n", cpu->tsc_off, cpu->eip, cpu->edx_eax());
	break;
      case 0x174 ... 0x176:
	(&cpu->sysenter_cs)[cpu->ecx - 0x174] = cpu->edx_eax();
	msg.mtr_out |= MTD_SYSENTER;
	break;
      default:
	dprintf("unsupported wrmsr %x <-(%x:%x) at %x\n",  cpu->ecx, cpu->edx, cpu->eax, cpu->eip);
	GP0(msg);
      }
  }


  void handle_cpu_init(CpuMessage &msg, bool reset) {
    //dprintf("handle CPU %s event %x\n", reset ? "RESET" : "INIT", _event);
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
      // XXX PERF
    }


    // send LAPIC init
    LapicEvent msg2(reset ? LapicEvent::RESET : LapicEvent::INIT);
    bus_lapic.send(msg2);
  }


  /**
   * Prioritize different events.
   * Returns the events to clear.
   */
  void prioritize_events(CpuMessage &msg) {
    CpuState *cpu = msg.cpu;
    unsigned old_event = _event;
    //dprintf("%s %x inj %x\n", __func__, old_event, cpu->inj_info);

    if (!old_event)  return;
    msg.mtr_out  |= MTD_INJ;

    if (old_event & EVENT_DEBUG) {
      dprintf("state %x event %8x eip %8x\n", cpu->actv_state, old_event, cpu->eip);
      Cpu::atomic_and<volatile unsigned>(&_event, ~VCpu::EVENT_DEBUG);
      return;
    }

    if (old_event & EVENT_RESET) {
      Cpu::atomic_and<volatile unsigned>(&_event, ~VCpu::EVENT_RESET);
      handle_cpu_init(msg, true);

      // are we an AP and should go to the wait-for-sipi state?
      if (is_ap()) old_event |= EVENT_INIT;

      // fall through as we could have got an INIT or SIPI already
    }


    // INIT
    if (old_event & EVENT_INIT) {
      Cpu::atomic_and<volatile unsigned>(&_event, ~VCpu::EVENT_INIT);
      handle_cpu_init(msg, false);
      cpu->actv_state = 3;
      // fall through as we could have got an SIPI already
    }


    // SIPI received?
    if (old_event & EVENT_SIPI) {
      cpu->eip          = 0;
      cpu->cs.sel       = _sipi & 0xff00;
      cpu->cs.base      = cpu->cs.sel << 4;
      cpu->actv_state   = 0;
      //dprintf("handle sipi %x event %x ip: %x:%x\n", _sipi, _event, cpu->cs.sel, cpu->eip);
      msg.mtr_out      |= MTD_CS_SS;
      Cpu::atomic_and<volatile unsigned>(&_event, ~VCpu::EVENT_SIPI);
      return;
    }

    // do block all other IRQs until we got a SIPI
    if (cpu->actv_state == 3)  return;

    // SMI
    if (old_event & EVENT_SMI && ~cpu->intr_state & 4) {
      dprintf("SMI received\n");
      Cpu::atomic_and<volatile unsigned>(&_event, ~VCpu::EVENT_SMI);
      cpu->actv_state = 0;
      // fall trough
    }

    // NMI
    if (old_event & EVENT_NMI && ~cpu->intr_state & 8 && !(cpu->intr_state & 3)) {
      cpu->inj_info = 0x80000202;
      cpu->actv_state = 0;
      Cpu::atomic_and<volatile unsigned>(&_event, ~VCpu::EVENT_NMI);
      return;
    }

    // if we can not inject interrupts or if we are in shutdown state return
    if (cpu->inj_info & 0x80000000 || cpu->intr_state & 0x3 || ~cpu->efl & 0x200 || cpu->actv_state == 2) return;


    // APIC interrupt?
    if (old_event & EVENT_FIXED) {
      LapicEvent msg2(LapicEvent::INTA);
      Cpu::atomic_and<volatile unsigned>(&_event, ~VCpu::EVENT_FIXED);
      if (bus_lapic.send(msg2)) {
	cpu->inj_info = msg2.value | 0x80000000;
      }
      cpu->actv_state = 0;
    }
    else if (old_event & EVENT_EXTINT) {
      Cpu::atomic_and<volatile unsigned>(&_event, ~VCpu::EVENT_EXTINT);
      MessageLegacy msg2(MessageLegacy::INTA, ~0u);
      if (_mb.bus_legacy.send(msg2)) {
	cpu->inj_info = msg2.value | 0x80000000;
      }
      cpu->actv_state = 0;
    }
  }

  void handle_irq(CpuMessage &msg) {
    //if (_event != 0x80)
    //dprintf("> handle_irq %x event %x\n", msg.mtr_out, _event);
    assert(msg.mtr_in & MTD_STATE);
    assert(msg.mtr_in & MTD_INJ);
    assert(msg.mtr_in & MTD_RFLAGS);

    prioritize_events(msg);
    msg.mtr_out |= MTD_STATE;
    //if (debug || CPUID_EDXb) dprintf("< handle_irq %x inj %x actv %x mtr %x/%x\n", _event, msg.cpu->inj_info, msg.cpu->actv_state, msg.mtr_in, msg.mtr_out);
  }


  void handle_ioin(CpuMessage &msg) {
    MessageIOIn msg2(MessageIOIn::Type(msg.io_order), msg.port);
    bool res = _mb.bus_ioin.send(msg2);

    Cpu::move(msg.dst, &msg2.value, msg.io_order);
    msg.mtr_out |= MTD_GPR_ACDB;
    //dprintf("in<%d> %4x %8x mtr %x\n", msg2.type, msg2.port, msg2.value, msg.mtr_out);

    static unsigned char debugioin[8192];
    if (!res && ~debugioin[msg.port >> 3] & (1 << (msg.port & 7))) {
      debugioin[msg.port >> 3] |= 1 << (msg.port & 7);
      dprintf("could not read from ioport %x eip %x cs %x-%x\n", msg.port, msg.cpu->eip, msg.cpu->cs.base, msg.cpu->cs.ar);
    }
  }


  void handle_ioout(CpuMessage &msg) {
    MessageIOOut msg2(MessageIOOut::Type(msg.io_order), msg.port, 0);
    Cpu::move(&msg2.value, msg.dst, msg.io_order);
    //dprintf("out<%d> %4x %8x\n", msg2.type, msg2.port, msg2.value);
    bool res = _mb.bus_ioout.send(msg2);

    static unsigned char debugioout[8192];
    if (!res && ~debugioout[msg.port >> 3] & (1 << (msg.port & 7))) {
      debugioout[msg.port >> 3] |= 1 << (msg.port & 7);
      dprintf("could not write %x to ioport %x eip %x\n", msg.cpu->eax, msg.port, msg.cpu->eip);
    }
  }

  /**
   * We received an asynchronous event. As code runs in many
   * threads, state updates have to be atomic!
   */
  void got_event(unsigned value) {
    COUNTER_INC("EVENT");

    //if (debug) dprintf("got event %x old %x\n", value, _event);
    if (value & DEASS_EXTINT) Cpu::atomic_and<volatile unsigned>(&_event, ~EVENT_EXTINT);
    if (!((_event ^ value) & (EVENT_MASK | EVENT_DEBUG))) return;

    // INIT or AP RESET - go to the wait-for-sipi state
    if ((value & EVENT_MASK) == EVENT_INIT ||
	(value & EVENT_MASK) == EVENT_RESET && is_ap())
      _sipi = 0;


    if ((value & EVENT_MASK) == EVENT_SIPI)
      /**
       * try to claim the SIPI field, if it is empty, we are waiting
       * for a SIPI. If it fails, somebody else was faster and we do
       * not wakeup the client.
       */
      if (Cpu::cmpxchg(&_sipi, 0, value)) return;
    Cpu::atomic_or<volatile unsigned>(&_event, STATE_WAKEUP | (value & (EVENT_MASK | EVENT_DEBUG)));
    //if ((value & EVENT_MASK) == EVENT_SIPI)
    //dprintf("wakeup thread %x sipi %x\n", _event, _sipi);
    MessageHostOp msg(MessageHostOp::OP_VCPU_RELEASE, _hostop_id, _event & STATE_BLOCK);
    _mb.bus_hostop.send(msg);
  }

public:
  /**
   * Forward MEM requests to the motherboard.
   */
  bool receive(MessageMem &msg) { return _mb.bus_mem.send(msg, true); }
  bool receive(MessageMemRegion &msg) { return _mb.bus_memregion.send(msg, true); }


  bool receive(CpuEvent &msg) { got_event(msg.value); return true; }
  bool receive(MessageLegacy &msg) {
    if (msg.type == MessageLegacy::RESET) {
      got_event(EVENT_RESET);
      return true;
    }

    // BSP receives only legacy signals if the LAPIC is disabled
    if (is_ap() || CPUID_EDX1 & (1 << 9)) return false;

    if (msg.type == MessageLegacy::EXTINT)
      got_event(EVENT_EXTINT);
    else if (msg.type == MessageLegacy::DEASS_EXTINT)
      got_event(DEASS_EXTINT);
    else if (msg.type == MessageLegacy::NMI)
      got_event(EVENT_NMI);
    else if (msg.type == MessageLegacy::INIT)
      got_event(EVENT_INIT);
    else return false;
    return true;
  }



  bool receive(CpuMessage &msg) {
    //dprintf("CPU Message %d %x/%x\n", msg.type, msg.mtr_in, msg.mtr_out);
    switch (msg.type) {
    case CpuMessage::TYPE_CPUID:    return handle_cpuid(msg);
    case CpuMessage::TYPE_CPUID_WRITE:
      {
	unsigned reg = (msg.nr << 4) | msg.reg | msg.nr & 0x80000000;
	unsigned old;
	if (CPUID_read(reg, old) && CPUID_write(reg, (old & msg.mask) | msg.value)) {
	  CPUID_read(reg, old);
	  //dprintf("CPUID %x value %x mask %x meant %x\n", reg, old, msg.mask, msg.value);
	  return true;
	}
	return false;
      };
    case CpuMessage::TYPE_RDTSC:
      handle_rdtsc(msg);
      return true;
    case CpuMessage::TYPE_RDMSR:
      handle_rdmsr(msg);
      return true;
    case CpuMessage::TYPE_WRMSR:
      handle_wrmsr(msg);
      return true;
    case CpuMessage::TYPE_IOIN:
      handle_ioin(msg);
      return true;
    case CpuMessage::TYPE_IOOUT:
      handle_ioout(msg);
      return true;
    case CpuMessage::TYPE_INIT:
      got_event(EVENT_INIT);
      return true;
    case CpuMessage::TYPE_TRIPLE:
      msg.cpu->actv_state = 2;
      break;
    case CpuMessage::TYPE_HLT:
      assert(!msg.cpu->actv_state);
      msg.cpu->actv_state = 1;
      break;
    case CpuMessage::TYPE_CHECK_IRQ:
      //if (debug) dprintf("CHECK IRQ %x\n", _event);
      // we handle it later on
      break;
    case CpuMessage::TYPE_CALC_IRQWINDOW:
      {
	assert(msg.mtr_out & MTD_INJ);
	unsigned new_event = _event;
	msg.cpu->inj_info &= ~INJ_WIN;
	if (new_event & (EVENT_EXTINT | EVENT_FIXED))  msg.cpu->inj_info |= INJ_IRQWIN;
	if (new_event & EVENT_NMI)                     msg.cpu->inj_info |= INJ_NMIWIN;
	//if (debug) dprintf("CHECK IRQ %x inj %x\n", _event, msg.cpu->inj_info);
      }
      return true;
    case CpuMessage::TYPE_SINGLE_STEP:
    case CpuMessage::TYPE_WBINVD:
    case CpuMessage::TYPE_INVD:
    default:
      return false;
    }

    //dprintf("CPU Message %d utcb %p eip %x\n", msg.type, msg.cpu, msg.cpu->eip);
    // handle IRQ injection
    for (handle_irq(msg); msg.cpu->actv_state & 0x3; handle_irq(msg)) {
      MessageHostOp msg2(MessageHostOp::OP_VCPU_BLOCK, _hostop_id);
      Cpu::atomic_or<volatile unsigned>(&_event, STATE_BLOCK);
      if (~_event & STATE_WAKEUP) _mb.bus_hostop.send(msg2);
      Cpu::atomic_and<volatile unsigned>(&_event, ~(STATE_BLOCK | STATE_WAKEUP));
    }
    //if (debug) dprintf("CPU Message %d utcb %p eip %x event %x inj %x\n", msg.type, msg.cpu, msg.cpu->eip, _event, msg.cpu->inj_info);
    return true;
  }

  VirtualCpu(VCpu *_last, Motherboard &mb) : VCpu(_last), _mb(mb), _event(0), _sipi(~0u)  {
    MessageHostOp msg(this);
    if (!mb.bus_hostop.send(msg)) Logging::panic("could not create VCpu backend.");
    _hostop_id = msg.value;

    // add to the busses
    executor. add(this, &VirtualCpu::receive_static<CpuMessage>);
    bus_event.add(this, &VirtualCpu::receive_static<CpuEvent>);
    mem.      add(this, &VirtualCpu::receive_static<MessageMem>);
    memregion.add(this, &VirtualCpu::receive_static<MessageMemRegion>);
    mb.bus_legacy.add(this, &VirtualCpu::receive_static<MessageLegacy>);

    CPUID_reset();
 }
};

PARAM(vcpu,
      mb.last_vcpu = new VirtualCpu(mb.last_vcpu, mb);
      ,
      "vcpu - create a new VCPU")

#else
REGSET(CPUID,
       REG_RW(CPUID_EAX0,  0x00, 2, ~0u,)
       REG_RW(CPUID_EBX0,  0x01, 0, ~0u,)
       REG_RW(CPUID_ECX0,  0x02, 0, ~0u,)
       REG_RW(CPUID_EDX0,  0x03, 0, ~0u,)
       REG_RW(CPUID_EAX1,  0x10, 0x673, ~0u,)
       REG_RW(CPUID_EBX1,  0x11, 0, ~0u,)
       REG_RW(CPUID_ECX1,  0x12, 0, ~0u,)
       REG_RW(CPUID_EDX1,  0x13, 0, ~0u,)
       REG_RW(CPUID_EDXb,  0xb3, 0, ~0u,)
       REG_RW(CPUID_EAX80, 0x80000000, 0x80000004, ~0u,)
       REG_RW(CPUID_ECX81, 0x80000012, 0x100000, ~0u,)
       REG_RW(CPUID_EAX82, 0x80000020, 0, ~0u,)
       REG_RW(CPUID_EBX82, 0x80000021, 0, ~0u,)
       REG_RW(CPUID_ECX82, 0x80000022, 0, ~0u,)
       REG_RW(CPUID_EDX82, 0x80000023, 0, ~0u,)
       REG_RW(CPUID_EAX83, 0x80000030, 0, ~0u,)
       REG_RW(CPUID_EBX83, 0x80000031, 0, ~0u,)
       REG_RW(CPUID_ECX83, 0x80000032, 0, ~0u,)
       REG_RW(CPUID_EDX83, 0x80000033, 0, ~0u,)
       REG_RW(CPUID_EAX84, 0x80000040, 0, ~0u,)
       REG_RW(CPUID_EBX84, 0x80000041, 0, ~0u,)
       REG_RW(CPUID_ECX84, 0x80000042, 0, ~0u,)
       REG_RW(CPUID_EDX84, 0x80000043, 0, ~0u,))
#endif
