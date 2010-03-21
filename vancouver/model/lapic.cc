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

#ifndef REGBASE
#include "nul/motherboard.h"
#include "nul/vcpu.h"

/**
 * X2Apic model.
 * State: unstable
 * Features: MEM and MSR access, MSR-base and CPUID, LVT, LINT0/1, EOI, prioritize IRQ, error, RemoteEOI
 * Missing:  reset,  IPI, timer, x2apic mode
 */
class X2Apic : public StaticReceiver<X2Apic>
{
#define REGBASE "../model/lapic.cc"
#include "model/reg.h"
  enum {
    LVT_MASK_BIT = 16,
    LVT_LEVEL = 1 << 15,
    OFS_ISR   = 0,
    OFS_TMR   = 256,
    OFS_IRR   = 512,
    LVT_BASE  = _TIMER_offset,
  };
  VCpu *_vcpu;
  DBus<MessageApic> &_bus_apic;
  unsigned long long _msr;
  bool in_x2apic_mode;
  unsigned _vector[8*3];
  unsigned _esr_shadow;
  unsigned _isrv;
  bool     _lvtds[6];
  bool     _lvtrirr[6];

  /**
   * Update the APIC base MSR.
   */
  bool update_msr(unsigned long long value) {
    const unsigned long long mask = ((1ull << (Config::PHYS_ADDR_SIZE)) - 1) &  ~0x6ffull;
    if (value & ~mask) return false;
    if (_msr ^ value & 0x800) {
      CpuMessage msg(1, 3, 2, _msr & 0x800 ? 0 : 2);
      _vcpu->executor.send(msg);
    }
    // make the BSP flag read-only
    _msr = (value & ~0x100ull) | (_msr & 0x100);
    return true;
  }

  void send_ipi(unsigned icr, unsigned dst) {
    Logging::panic("%s", __PRETTY_FUNCTION__);
  }

  unsigned get_highest_bit(unsigned bit_offset) {
    for (int i=7; i >=0; i--) {
      unsigned value = _vector[(bit_offset >> 5) + i];
      if (value) return (i << 5) | Cpu::bsr(value);
    }
    return 0;
  }

  unsigned processor_prio() {
    if (_TPR >= (_isrv & 0xf0))
      return _TPR;
    return _isrv & 0xf0;
  }

  unsigned prioritize_irq() {
    unsigned irrv = get_highest_bit(OFS_IRR);
    if ((irrv & 0xf0) > processor_prio()) return irrv;
    return 0;
  }

  void update_irqs() {
    CpuEvent msg(VCpu::EVENT_FIXED);
    if (prioritize_irq()) _vcpu->bus_event.send(msg);
  }


  void set_error(unsigned bit) {
    if (_esr_shadow & (1<< bit)) return;
    Cpu::atomic_set_bit(&_esr_shadow, bit);
    trigger_lvt(_ERROR_offset - LVT_BASE);
  }


  void accept_vector(unsigned value, bool level) {
    unsigned vector = value & 0xff;

    // lower vectors are reserved
    if (vector < 16) set_error(6);
    else {
      Cpu::atomic_set_bit(_vector, OFS_IRR + vector);
      Cpu::atomic_set_bit(_vector, OFS_TMR + vector, level);
    }
    update_irqs();
  }

  void broadcast_eoi(unsigned vector) {
    // we clear our LVT entries first
    if (vector == (_LINT0 & 0xff)) _lvtrirr[_LINT0_offset - LVT_BASE] = false;
    if (vector == (_LINT1 & 0xff)) _lvtrirr[_LINT1_offset - LVT_BASE] = false;

    MessageApic msg(vector);
    _bus_apic.send(msg);
  }


  bool register_read(unsigned num, unsigned &value) {
    bool res = false;
    switch (num) {
    case 0x0a:
      value = processor_prio();
      res = true;
      break;
    case 0x10 ... 0x18:
      value = _vector[num - 0x10];
      res = true;
      break;
    default:
      res = X2Apic_read(num, value);
    }
    if (in_range(num, LVT_BASE, 6)) {
      if (_lvtds[num - LVT_BASE])    value |= 1 << 12;
      if (_lvtrirr[num - LVT_BASE])  value |= 1 << 14;
    }
    if (!res) set_error(7);
    return res;
  }


  bool register_write(unsigned num, unsigned value) {
    bool res;
    switch (num) {
    case 0x9: // APR
    case 0xc: // RRD
      // the accesses are ignored
      return true;
    case 0xb: // EOI
      {
	if (_isrv) {
	  Cpu::set_bit(_vector, OFS_ISR + _isrv, false);
	  if (Cpu::get_bit(_vector, OFS_TMR + _isrv)) broadcast_eoi(_isrv);

	  _isrv = get_highest_bit(OFS_ISR);
	  update_irqs();
	}
      }
      return true;
    default:
      res = X2Apic_write(num, value, true);
    }
    if (!res) set_error(7);
    return true;
  }


  bool trigger_lvt(unsigned num){
    assert(num < 6);
    unsigned lvt;
    X2Apic_read(num + LVT_BASE, lvt);


    unsigned event = (lvt >> 8) & 7;
    bool level =  (lvt & LVT_LEVEL && event == VCpu::EVENT_FIXED) || (event == VCpu::EVENT_EXTINT);

    // level && masked - set delivery status bit
    if (lvt & (1 << LVT_MASK_BIT)) {
      if (level) _lvtds[num] = true;
      return true;
    }

    // perf IRQs are auto masked
    if (num == (_PERF_offset - LVT_BASE)) Cpu::atomic_set_bit(&_PERF, LVT_MASK_BIT);

    switch (event) {
    case VCpu::EVENT_FIXED:
      // set Remote IRR on level triggered IRQs
      if (level) _lvtrirr[num] = true;
      accept_vector(lvt, level);
      break;
    case VCpu::EVENT_SMI:
    case VCpu::EVENT_NMI:
    case VCpu::EVENT_INIT :
    case VCpu::EVENT_EXTINT:
      {
	CpuEvent msg(event);
	_vcpu->bus_event.send(msg);
      }
      break;
    default:
      // other encodings (LOWEST_PRIO, SIPI, REMOTE_READ) are reserved, thus we simply drop them
      break;
    }

    // we have delivered it
    _lvtds[num] = false;
    return true;
  }

public:
  bool  receive(MessageMem &msg)
  {
    if (!in_range(_msr & ~0xfff, msg.phys, 0x1000)) return false;
    if ((msg.phys & 0xf) || (msg.phys & 0xfff) >= 0x40*4) return false;
    if (msg.read) register_read((msg.phys >> 4) & 0x3f, *msg.ptr);
    else          register_write((msg.phys >> 4) & 0x3f, *msg.ptr);
    return true;
  }

  bool  receive(MessageMemRegion &msg)
  {
    if ((_msr >> 12) != msg.page) return false;
    /**
     * we return true without setting msg.ptr and thus nobody else can
     * claim this region
     */
    msg.start_page = msg.page;
    msg.count = 1;
    return true;
  }


  /**
   * An INTA cycle
   */
  bool  receive(CpuEvent &msg) {
    unsigned irrv = prioritize_irq();
    if (irrv) {
      assert(irrv > _isrv);
      Cpu::atomic_set_bit(_vector, OFS_IRR + irrv, false);
      Cpu::set_bit(_vector, OFS_ISR + irrv);
      _isrv = irrv;
      msg.value = irrv;
    } else
      msg.value = _SVR & 0xff;
    return true;
  }


  bool  receive(CpuMessage &msg) {
    switch (msg.type) {
    case CpuMessage::TYPE_RDMSR:
      // handle APIC base MSR
      if (msg.cpu->ecx == 0x1b) {
	msg.cpu->edx_eax(_msr);
	return true;
      }
      // check whether the register is available
      if (!in_range(msg.cpu->ecx, 0x800, 64)
	  || !in_x2apic_mode
	  || msg.cpu->ecx == 0x831
	  || msg.cpu->ecx == 0x80e) return false;

      // read register
      if (!register_read(msg.cpu->ecx & 0x3f, msg.cpu->eax)) return false;

      // only the ICR has an upper half
      msg.cpu->edx = (msg.cpu->ecx == 0x830) ? _ICR1 : 0;
      return true;
    case CpuMessage::TYPE_WRMSR: {
      // handle APIC base MSR
      if (msg.cpu->ecx == 0x1b)  return update_msr(msg.cpu->edx_eax());
      // check whether the register is available
      if (!in_range(msg.cpu->ecx, 0x800, 64)
	  || !in_x2apic_mode
	  || msg.cpu->ecx == 0x831
	  || msg.cpu->ecx == 0x80e
	  || msg.cpu->edx && msg.cpu->ecx != 0x830) return false;

      // self IPI?
      if (msg.cpu->ecx == 0x83f && msg.cpu->eax < 0x100 && !msg.cpu->edx) {
	send_ipi(0x40000| msg.cpu->eax, 0);
	return true;
      }

      unsigned old_ICR1 = _ICR1;
      // write upper half of the ICR
      if (msg.cpu->ecx == 0x830) _ICR1 = msg.cpu->edx;

      // write lower half in strict mode with reserved bit checking
      if (!register_write(msg.cpu->ecx & 0x3f, msg.cpu->eax)) {
	_ICR1 = old_ICR1;
	return false;
      }
      return true;
    }
    case CpuMessage::TYPE_CPUID:
    case CpuMessage::TYPE_CPUID_WRITE:
    case CpuMessage::TYPE_RDTSC:
    default:
      return false;
    }
  }



  bool  receive(MessageLegacy &msg) {
    if (msg.type == MessageLegacy::RESET)
      return update_msr(_vcpu->is_ap() ? 0xfee00800 : 0xfee00900);
    if (_vcpu->is_ap()) return false;

    // the BSP gets the legacy PIC output and NMI on LINT0/1
    if (msg.type == MessageLegacy::EXTINT)
      return trigger_lvt(_LINT0_offset - LVT_BASE);
    if (msg.type == MessageLegacy::NMI)
      return trigger_lvt(_LINT1_offset - LVT_BASE);
    return false;
  }


  X2Apic(VCpu *vcpu, DBus<MessageApic> &bus_apic, unsigned initial_apic_id) : _vcpu(vcpu), _bus_apic(bus_apic), in_x2apic_mode(false) {
    _ID = initial_apic_id;

    // propagate initial APIC id
    CpuMessage msg1(1,  1, 0xffffff, _ID << 24);
    CpuMessage msg2(11, 3, 0, _ID);
    _vcpu->executor.send(msg1);
    _vcpu->executor.send(msg2);
  }
};


PARAM(x2apic, {
    if (!mb.last_vcpu) Logging::panic("no VCPU for this APIC");

    X2Apic *dev = new X2Apic(mb.last_vcpu, mb.bus_apic, argv[0]);
    mb.bus_legacy.add(dev, &X2Apic::receive_static<MessageLegacy>);
    mb.last_vcpu->executor.add(dev, &X2Apic::receive_static<CpuMessage>);
    mb.last_vcpu->mem.add(dev, &X2Apic::receive_static<MessageMem>);
    mb.last_vcpu->memregion.add(dev, &X2Apic::receive_static<MessageMemRegion>);
    mb.last_vcpu->bus_lapic.add(dev, &X2Apic::receive_static<CpuEvent>);
  },
  "x2apic:inital_apic_id - provide an x2 APIC for every CPU",
  "Example: 'x2apic:2'");
#else
REGSET(X2Apic,
       REG_RW(_ID,            0x02,          0, 0)
       REG_RO(_VERSION,       0x03, 0x00050014)
       REG_RW(_TPR,           0x08,          0, 0xff)
       REG_RW(_SVR,           0x0f, 0x000000ff, 0x11ff)
       REG_WR(_ESR,           0x28,          0, 0,          0, 0, _ESR = Cpu::xchg(&_esr_shadow, 0); )
       REG_WR(_ICR,           0x30,          0, 0x000ccfff, 0, 0, send_ipi(_ICR, _ICR1))
       REG_RW(_ICR1,          0x31,          0, 0xff000000)
       REG_RW(_TIMER,         0x32, 0x00010000, 0x300ff)
       REG_RW(_TERM,          0x33, 0x00010000, 0x107ff)
       REG_RW(_PERF,          0x34, 0x00010000, 0x107ff)
       REG_WR(_LINT0,         0x35, 0x00010000, 0x1a7ff, 0, 0, if (_lvtds[offset - LVT_BASE]) trigger_lvt(offset - LVT_BASE);)
       REG_WR(_LINT1,         0x36, 0x00010000, 0x1a7ff, 0, 0, if (_lvtds[offset - LVT_BASE]) trigger_lvt(offset - LVT_BASE);)
       REG_RW(_ERROR,         0x37, 0x00010000, 0x100ff)
       REG_RW(_INITIAL_COUNT, 0x38,          0, ~0u)
       REG_RW(_CURRENT_COUNT, 0x39,          0, ~0u)
       REG_RW(_DIVIDE_CONFIG, 0x3e,          0, 0xb))
#endif
