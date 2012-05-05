/** @file
 * Local APIC model.
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

#ifndef REGBASE
#include "nul/motherboard.h"
#include "nul/vcpu.h"

/**
 * Lapic model.
 *
 * State: testing
 * Features: MEM, MSR, MSR-base and CPUID, LVT, LINT0/1, EOI, prioritize IRQ, error, RemoteEOI, timer, IPI, lowest prio, reset, x2apic mode, BIOS ACPI tables
 * Missing:  focus checking, CR8/TPR setting
 * Difference:  no interrupt polarity, lowest prio is round-robin
 * Documentation: Intel SDM Volume 3a Chapter 10 253668-033.
 */
class Lapic : public DiscoveryHelper<Lapic>, public StaticReceiver<Lapic>
{
#define REGBASE "../model/lapic.cc"
#include "model/reg.h"
  enum {
    MAX_FREQ   = 200000000,
    LVT_MASK_BIT = 16,
    OFS_ISR   = 0,
    OFS_TMR   = 256,
    OFS_IRR   = 512,
    LVT_BASE  = _TIMER_offset,
    NUM_LVT   = 6,
    APIC_ADDR = 0xfee00000
  };

public:
  Motherboard &_mb;
private:
  VCpu *    _vcpu;
  unsigned  _initial_apic_id;
  unsigned  _timer;
  unsigned  _timer_clock_shift;

  // dynamic state
  unsigned  _timer_dcr_shift;
  timevalue _timer_start;
  unsigned long long _msr;
  unsigned  _vector[8*3];
  unsigned  _esr_shadow;
  unsigned  _isrv;
  bool      _lvtds[NUM_LVT];
  bool      _rirr[NUM_LVT];
  unsigned  _lowest_rr;


  bool sw_disabled() { return ~_SVR & 0x100; }
  bool hw_disabled() { return ~_msr & 0x800; }
  bool x2apic_mode() { return  (_msr & 0xc00) == 0xc00; }
  unsigned x2apic_ldr() { return ((_initial_apic_id & ~0xf) << 12) | ( 1 << (_initial_apic_id & 0xf)); }


  /**
   * Handle an INIT signal.
   */
  void init() {
    // INIT preserves the APIC ID and the LINT0 level
    bool lint0 = _lvtds[_LINT0_offset - LVT_BASE];
    unsigned old_id = _ID;

    // reset regs
    Lapic_reset();

    // init dynamic state
    _timer_dcr_shift = 1 + _timer_clock_shift;
    memset(_vector,  0, sizeof(_vector));
    memset(_lvtds,   0, sizeof(_lvtds));
    memset(_rirr,    0, sizeof(_rirr));
    _isrv = 0;
    _esr_shadow = 0;
    _lowest_rr = 0;


    _ID = old_id;
    _lvtds[_LINT0_offset - LVT_BASE] = lint0;


    update_irqs();
  }


  /**
   * Reset the APIC to some default state.
   */
  void reset() {
    init();
    _ID = _initial_apic_id << 24;
    _msr = 0;
    set_base_msr(APIC_ADDR | 0x800);
  }


  /**
   * Update the APIC base MSR.
   */
  bool set_base_msr(unsigned long long value) {
    const unsigned long long mask = ((1ull << (Config::PHYS_ADDR_SIZE)) - 1) &  ~0x2ffull;
    bool was_x2apic_mode = x2apic_mode();

    // check reserved bits and invalid state transitions
    if (value & ~mask || (value & 0xc00) == 0x400 || was_x2apic_mode && (value & 0xc00) == 0x800) return false;

    // disabled bit?
    if ((_msr ^ value) & 0x800) {
      bool apic_enabled = value & 0x800;
      CpuMessage msg[] = {
	// update CPUID leaf 1 EDX
	CpuMessage (1, 3, ~(1 << 9), apic_enabled << 9),
	// support for X2Apic
	CpuMessage(1,  2, ~(1 << 21), apic_enabled << 21),

      };
      for (unsigned i=0; i < sizeof(msg) / sizeof(*msg); i++)
	_vcpu->executor.send(msg[i]);
    }

    _msr = value;

    // init _ID on mode switches
    if (!was_x2apic_mode && x2apic_mode()) {
      _ID = _initial_apic_id;
      _ICR1 = 0;
    }

    // set them to default state if disabled
    if (hw_disabled()) init();
    return true;
  }

  /**
   * Checks whether a timeout should trigger and returns the current
   * counter value.
   */
  unsigned get_ccr(timevalue now) {
    if (!_ICT || !_timer_start)  return 0;

    timevalue delta = (now - _timer_start) >> _timer_dcr_shift;
    if (delta < _ICT)  return _ICT - delta;

    // we need to trigger the timer LVT
    trigger_lvt(_TIMER_offset - LVT_BASE);

    // one shot?
    if (~_TIMER & (1 << 17))  {
      _timer_start = 0;
      return 0;
    }

    // add periods to the time we started to detect the next overflow
    unsigned done = delta % _ICT;
    _timer_start += (delta - done) << _timer_dcr_shift;

    return _ICT - done;
  }

  /**
   * Reprogram a new host timer.
   */
  void update_timer(timevalue now) {
    unsigned value = get_ccr(now);
    if (!value || _TIMER & (1 << LVT_MASK_BIT)) return;
    MessageTimer msg(_timer, now + (value << _timer_dcr_shift));
    _mb.bus_timer.send(msg);
  }


  /**
   * We send an IPI.
   */
  bool send_ipi(unsigned icr, unsigned dst) {
    COUNTER_INC("IPI");

    unsigned shorthand = (icr >> 18) & 0x3;
    unsigned event =  1 << ((icr >> 8) & 7);
    bool self = shorthand == 1 || shorthand == 2;

    // no logical destination mode with shorthand
    if (shorthand) icr &= ~MessageApic::ICR_DM;

    // LOWEST does not work in x2apic mode
    if (event == VCpu::EVENT_LOWEST && x2apic_mode()) return set_error(4);

    // self IPIs can only be fixed
    if (self && event != VCpu::EVENT_FIXED
	// we can not send EXTINT and RRD
	|| event & (VCpu::EVENT_EXTINT | VCpu::EVENT_RRD)
	//  and INIT deassert messages
	|| event == VCpu::EVENT_INIT && ~icr & MessageApic::ICR_ASSERT

	/**
	 * This is a strange thing in the manual: lowest priority with
	 * a broadcast shorthand is invalid. But what about physical
	 * destination mode with dst=0xff?
	 */
	|| event == VCpu::EVENT_LOWEST && shorthand == 2)
      return false;

    // send vector error check
    if ((event & (VCpu::EVENT_FIXED | VCpu::EVENT_LOWEST)) && (icr & 0xff) < 16)
      return set_error(5);

    // self IPI?
    if (shorthand == 1) dst = _ID;

    // broadcast?
    if (shorthand & 2) dst = ~0u;

    if (!x2apic_mode()) dst >>= 24;

    // level triggered IRQs are treated as edge triggered
    icr = icr & 0x4fff;
    if (event == VCpu::EVENT_LOWEST) {

      // we send them round-robin as EVENT_FIXED
      MessageApic msg((icr & ~0x700u), dst, 0);

      // we could set an send accept error here if nobody got the
      // message, but that is not supported in the P4...
      return _mb.bus_apic.send_rr(msg, _lowest_rr);
    }
    MessageApic msg(icr, dst, shorthand == 3 ? this : 0);
    return _mb.bus_apic.send(msg);
  }


  /**
   * Scan for the highest bit in the ISR or IRR.
   */
  unsigned get_highest_bit(unsigned bit_offset) {
    for (int i=7; i >=0; i--) {
      unsigned value = _vector[(bit_offset >> 5) + i];
      if (value) return (i << 5) | Cpu::bsr(value);
    }
    return 0;
  }

  /**
   * Calc the PPR.
   */
  unsigned processor_prio() {
    unsigned res = _isrv & 0xf0;
    if (_TPR >= res) res = _TPR;
    return res;
  }

  /**
   * Check whether there is an EXTINT in the LVTs or an IRQ above the
   * processor prio to inject.
   */
  unsigned prioritize_irq() {

    // EXTINT pending?
    for (unsigned i=0; i < NUM_LVT; i++) {
      unsigned lvt;
      Lapic_read(i + LVT_BASE, lvt);
      if (_lvtds[i] && ((1 << ((lvt >> 8) & 7)) == VCpu::EVENT_EXTINT))
	return 0x100 | i;
    }

    unsigned irrv = get_highest_bit(OFS_IRR);
    if (irrv && (irrv & 0xf0) > processor_prio()) return irrv;
    return 0;
  }


  /**
   * Send upstream to the CPU that we have an IRQ.
   */
  void update_irqs() {
    COUNTER_INC("update irqs");


    if (hw_disabled()) return;

    // send our output line level upstream
    CpuEvent msg(VCpu::EVENT_INTR);
    if (!prioritize_irq()) msg.value = VCpu::DEASS_INTR;
    _vcpu->bus_event.send(msg);
  }


  /**
   * Indicate an error and trigger the error LVT.
   */
  bool set_error(unsigned bit) {
    /**
     * Make sure we do not loop, if the ERROR LVT has also an invalid
     * vector programmed.
     */
    if (_esr_shadow & (1<< bit)) return true;
    Cpu::atomic_set_bit(&_esr_shadow, bit);
    return trigger_lvt(_ERROR_offset - LVT_BASE);
  }

  /**
   * Accept a fixed vector in the IRR.
   */
  void accept_vector(unsigned char vector, bool level, bool value) {

    // lower vectors are reserved
    if (vector < 16) set_error(6);
    else {
      Cpu::atomic_set_bit(_vector, OFS_IRR + vector, !level || value);
      Cpu::atomic_set_bit(_vector, OFS_TMR + vector, level);
    }
    update_irqs();
  }

  /**
   * Broadcast an EOI on the bus if it is level triggered.
   */
  void broadcast_eoi(unsigned vector) {
    if (!Cpu::get_bit(_vector, OFS_TMR + _isrv)) return;

    // we clear our LVT entries first
    if (vector == (_LINT0 & 0xff)) _rirr[_LINT0_offset - LVT_BASE] = false;
    if (vector == (_LINT1 & 0xff)) _rirr[_LINT1_offset - LVT_BASE] = false;

    // broadcast suppression?
    if (_SVR & 0x1000) return;

    MessageMem msg(false, MessageApic::IOAPIC_EOI, &vector);
    _mb.bus_mem.send(msg);
  }

  /**
   *
   */

  bool register_read(unsigned offset, unsigned &value) {
    COUNTER_INC("lapic read");
    bool res = true;
    switch (offset) {
    case 0x0a:
      value = processor_prio();
      break;
    case 0x10 ... 0x27:
      value = _vector[offset - 0x10];
      break;
    case 0x39:
      COUNTER_INC("lapic ccr");
      value = get_ccr(_mb.clock()->time());
      break;
    default:
      if (!(res = Lapic_read(offset, value))) set_error(7);
    }

    // LVT bits
    if (in_range(offset, LVT_BASE, NUM_LVT)) {
      value &= ~(1 << 12);
      if (_lvtds[offset - LVT_BASE]) value |= 1 << 12;
      if (_rirr[offset - LVT_BASE])  value |= MessageApic::ICR_ASSERT;
    }
    return res;
  }


  bool register_write(unsigned offset, unsigned value, bool strict) {
    bool res;
    COUNTER_INC("lapic write");

    // XXX
    if (sw_disabled() && in_range(offset, LVT_BASE, NUM_LVT))  value |= 1 << 16;
    switch (offset) {
    case 0x9: // APR
    case 0xc: // RRD
      // the accesses are ignored
      return true;
    case 0xb: // EOI
      COUNTER_INC("lapic eoi");
      if (strict && value) return false;
      if (_isrv) {
	Cpu::set_bit(_vector, OFS_ISR + _isrv, false);
	broadcast_eoi(_isrv);

	// if we eoi the timer IRQ, rearm the timer
	if (_isrv == (_TIMER & 0xff)) update_timer(_mb.clock()->time());

	_isrv = get_highest_bit(OFS_ISR);
	update_irqs();
      }
      return true;
    default:
      if (!(res = Lapic_write(offset, value, strict))) {
	if (offset != 3)  Logging::printf("LAPIC write %x at offset %x failed\n", value, offset);
	set_error(7);
      }
    }

    // mask all entries on SVR writes
    if (offset == _SVR_offset && sw_disabled())
      for (unsigned i=0; i < NUM_LVT; i++) {
	register_read (i + LVT_BASE, value);
	register_write(i + LVT_BASE, value, false);
      }

    // do side effects of a changed LVT entry
    if (in_range(offset, LVT_BASE, NUM_LVT)) {
      if (_lvtds[offset - LVT_BASE]) trigger_lvt(offset - LVT_BASE);
      if (offset == _TIMER_offset)   update_timer(_mb.clock()->time());
      update_irqs();
    }
    return res;
  }


  /**
   * Trigger an LVT entry.
   */
  bool trigger_lvt(unsigned num){
    assert(num < NUM_LVT);
    unsigned lvt;
    Lapic_read(num + LVT_BASE, lvt);
    if (!num) COUNTER_INC("LVT0");


    unsigned event = 1 << ((lvt >> 8) & 7);
    bool level =  (lvt & MessageApic::ICR_LEVEL && event == VCpu::EVENT_FIXED) || (event == VCpu::EVENT_EXTINT);

    // do not accept more IRQs if no EOI was performed
    if (_rirr[num]) return true;

    // level - set delivery status bit
    if (level) _lvtds[num] = true;

    // masked irq?
    if (lvt & (1 << LVT_MASK_BIT))  return true;


    // perf IRQs are auto masked
    if (num == (_PERF_offset - LVT_BASE)) Cpu::atomic_set_bit(&_PERF, LVT_MASK_BIT);

    if (event == VCpu::EVENT_FIXED) {
      // set Remote IRR on level triggered IRQs
      _rirr[num] = level;
      accept_vector(lvt, level, true);

      // we have delivered it
      // XXX what about level triggered LVT0 DS?
      _lvtds[num] = false;
    }
    else if (event & VCpu::EVENT_EXTINT)
      // _lvtds is set, thus update_irqs will propagate this upstream
      update_irqs();
    else if (event & (VCpu::EVENT_SMI | VCpu::EVENT_NMI | VCpu::EVENT_INIT)) {
      CpuEvent msg(event);
      _vcpu->bus_event.send(msg);
    }

    /**
     * It is not defined how invalid Delivery Modes are handled. We
     * simply drop SIPI, RRD and LOWEST here.
     */
    return true;
  }


  /**
   * Check whether we should accept the message.
   */
  bool accept_message(MessageApic &msg) {
    // XXX what about sw_disabled?
    if (hw_disabled())   return false;
    if (msg.ptr == this) return false;

    if (x2apic_mode()) {
      // broadcast?
      if (msg.dst == ~0u)    return true;
      // physical DM
      if (~msg.icr & MessageApic::ICR_DM) return msg.dst == _ID;

      // logical DM
      unsigned ldr = x2apic_ldr();
      return !((ldr ^ msg.dst) & 0xffff0000) && ldr & msg.dst & 0xffff;
    }

    unsigned dst = msg.dst << 24;

    // broadcast
    if (dst == 0xff000000)  return true;

    // physical DM
    if (~msg.icr & MessageApic::ICR_DM) return dst == _ID;

    // flat mode
    if ((_DFR >> 28) == 0xf) return !!(_LDR & dst);

    // cluster mode
    return !((_LDR ^ dst) & 0xf0000000) && _LDR & dst & 0x0fffffff;
  }


public:
  /**
   * Receive MMIO access.
   */
  bool  receive(MessageMem &msg)
  {
    if (((_msr & 0xc00) != 0x800) || !in_range(msg.phys, _msr & ~0xfffull, 0x1000)) return false;
    if ((msg.phys & 0xf) || (msg.phys & 0xfff) >= 0x400) return false;


    if (msg.read)
      register_read((msg.phys >> 4) & 0x3f, *msg.ptr);
    else
      register_write((msg.phys >> 4) & 0x3f, *msg.ptr, false);
    return true;
  }

  /**
   * Receive MMIO map request.
   */
  bool  receive(MessageMemRegion &msg)
  {
    if ((_msr >> 12) != msg.page) return false;
    /**
     * We return true without setting msg.ptr and thus nobody else can
     * claim this region.
     */
    msg.start_page = msg.page;
    msg.count = 1;
    return true;
  }

  /**
   * Timeout for the APIC timer.
   */
  bool  receive(MessageTimeout &msg) {
    if (hw_disabled() || msg.nr != _timer) return false;

    // no need to call update timer here, as the CPU needs to do an
    // EOI first
    get_ccr(_mb.clock()->time());
    return true;
  }


  /**
   * Receive an IPI.
   */
  bool  receive(MessageApic &msg) {
    if (!accept_message(msg)) return false;
    assert(!(msg.icr & ~0xcfff));
    unsigned event = 1 << ((msg.icr >> 8) & 7);

    assert(event != VCpu::EVENT_RRD);
    assert(event != VCpu::EVENT_LOWEST);

    if (event == VCpu::EVENT_FIXED)
      accept_vector(msg.icr, msg.icr & MessageApic::ICR_LEVEL, msg.icr & MessageApic::ICR_ASSERT);
    else {
      if (event == VCpu::EVENT_SIPI) event |= (msg.icr & 0xff) << 8;

      // forward INIT, SIPI, SMI, NMI and EXTINT directly to the CPU core
      CpuEvent msg(event);
      _vcpu->bus_event.send(msg);
    }
    return true;
  }

  /**
   * Receive INTA cycle or RESET from the CPU.
   */
  bool  receive(LapicEvent &msg) {
    if (!hw_disabled() && msg.type == LapicEvent::INTA) {
      unsigned irrv = prioritize_irq();

      if (irrv & 0x100) {
	// EXTINT from some LVT entry? -> we have delivered them
	// XXX what about level triggered _LINT0-ds?
	_lvtds[irrv & 0xff] = false;

	// the VCPU will send the INTA to the PIC itself, so nothing todo for us
	return false;
      }
      else if (irrv) {
	assert(irrv > _isrv);
	Cpu::atomic_set_bit(_vector, OFS_IRR + irrv, false);
	Cpu::set_bit(_vector, OFS_ISR + irrv);
	_isrv = irrv;
	msg.value = irrv;
      } else
	msg.value = _SVR & 0xff;
      update_irqs();
    }
    else if (msg.type == LapicEvent::RESET)
      reset();
    else if (msg.type == LapicEvent::INIT)
      init();
    return true;
  }


  /**
   * Receive RDMSR and WRMSR messages.
   */
  bool  receive(CpuMessage &msg) {
    if (msg.type == CpuMessage::TYPE_RDMSR) {
      msg.mtr_out |= MTD_GPR_ACDB;

      // handle APIC base MSR
      if (msg.cpu->ecx == 0x1b) { msg.cpu->edx_eax(_msr); return true; }

      // check whether the register is available
      if (!in_range(msg.cpu->ecx, 0x800, 64)
	  || !x2apic_mode()
	  || msg.cpu->ecx == 0x831
	  || msg.cpu->ecx == 0x80e) return false;

      if (msg.cpu->ecx == 0x80d) {  msg.cpu->edx_eax(x2apic_ldr()); return true;  }

      // read register
      if (!register_read(msg.cpu->ecx & 0x3f, msg.cpu->eax)) return false;

      // only the ICR has an upper half
      msg.cpu->edx = (msg.cpu->ecx == 0x830) ? _ICR1 : 0;
      return true;
    }

    // WRMSR
    if (msg.type == CpuMessage::TYPE_WRMSR) {

      // handle APIC base MSR
      if (msg.cpu->ecx == 0x1b)  return set_base_msr(msg.cpu->edx_eax());


      // check whether the register is available
      if (!in_range(msg.cpu->ecx, 0x800, 64)
	  || !x2apic_mode()
	  || msg.cpu->ecx == 0x831
	  || msg.cpu->ecx == 0x802
	  || msg.cpu->ecx == 0x80d
	  || msg.cpu->ecx == 0x80e
	  || msg.cpu->edx && msg.cpu->ecx != 0x830) return false;

      // self IPI?
      if (msg.cpu->ecx == 0x83f && msg.cpu->eax < 0x100 && !msg.cpu->edx)
	return send_ipi(0x40000 | msg.cpu->eax, 0);

      // write upper half of the ICR first
      unsigned old_ICR1 = _ICR1;
      if (msg.cpu->ecx == 0x830) _ICR1 = msg.cpu->edx;

      // write lower half in strict mode with reserved bit checking
      if (!register_write(msg.cpu->ecx & 0x3f, msg.cpu->eax, true)) {
	_ICR1 = old_ICR1;
	Logging::printf("FAILED %x strict\n", msg.cpu->ecx);
	return false;
      }
      return true;
    }
    return false;
  }


  /**
   * Legacy pins.
   */
  bool  receive(MessageLegacy &msg) {

    // the legacy PIC output is level triggered and wired to LINT0
    if (msg.type == MessageLegacy::INTR) {
      _lvtds[_LINT0_offset - LVT_BASE] = true;
      if (!hw_disabled()) trigger_lvt(_LINT0_offset - LVT_BASE);
    }
    else if (msg.type == MessageLegacy::DEASS_INTR) {
      _lvtds[_LINT0_offset - LVT_BASE] = false;
      update_irqs();
    }
    // NMIs are received on LINT1
    else if (!hw_disabled() && msg.type == MessageLegacy::NMI)
      return trigger_lvt(_LINT1_offset - LVT_BASE);
    else
      return false;
    return true;
  }


  void discovery() {

    unsigned value = 0;
    discovery_read_dw("APIC", 36, value);
    unsigned length = discovery_length("APIC", 44);
    if (value == 0) {

      // NMI is connected to LINT1 on all LAPICs
      discovery_write_dw("APIC", length + 0, 0x00ff0604 | (_initial_apic_id << 24), 4);
      discovery_write_dw("APIC", length + 4,     0x0100, 2);
      length += 6;


      // NMI is connected to LINT1 on all x2APICs
      discovery_write_dw("APIC", length + 0, 0x00000c0a | (_initial_apic_id << 24), 4);
      discovery_write_dw("APIC", length + 4, 0xffffffff, 4);
      discovery_write_dw("APIC", length + 8,          1, 4);
      length += 12;


      // write the default APIC address to the MADT
      discovery_write_dw("APIC",  36,    APIC_ADDR, 4);

      // and that we have legacy PICs
      discovery_write_dw("APIC",  40,    1, 4);
    }

    // add the LAPIC structure to the MADT
    if (_initial_apic_id  < 255) {

      discovery_write_dw("APIC", length, (_initial_apic_id << 24) | 0x0800, 4);
      discovery_write_dw("APIC", length + 4, 1, 4);
    }
    else {
      discovery_write_dw("APIC", length +  0, 0x1009, 4);
      discovery_write_dw("APIC", length +  4, _initial_apic_id, 4);
      discovery_write_dw("APIC", length +  8, 1, 4);
      discovery_write_dw("APIC", length + 12, _initial_apic_id, 4);
    }
  }


  Lapic(Motherboard &mb, VCpu *vcpu, unsigned initial_apic_id, unsigned timer) : _mb(mb), _vcpu(vcpu), _initial_apic_id(initial_apic_id), _timer(timer)
  {
    // find a FREQ that is not too high
    for (_timer_clock_shift=0; _timer_clock_shift < 32; _timer_clock_shift++)
      if ((_mb.clock()->freq() >> _timer_clock_shift) <= MAX_FREQ) break;

    Logging::printf("LAPIC freq %lld\n", _mb.clock()->freq() >> _timer_clock_shift);
    CpuMessage msg[] = {
      // propagate initial APIC id
      CpuMessage(1,  1, 0xffffff, _initial_apic_id << 24),
      CpuMessage(11, 3, 0, _initial_apic_id),
      // support for APIC timer that does not sleep in C-states
      CpuMessage(6, 0, ~(1 << 2), 1 << 2),
    };
    for (unsigned i=0; i < sizeof(msg) / sizeof(*msg); i++)
      _vcpu->executor.send(msg[i]);

    reset();

    mb.bus_legacy.add(this,   receive_static<MessageLegacy>);
    mb.bus_apic.add(this,     receive_static<MessageApic>);
    mb.bus_timeout.add(this,  receive_static<MessageTimeout>);
    mb.bus_discovery.add(this,discover);
    vcpu->executor.add(this,  receive_static<CpuMessage>);
    vcpu->mem.add(this,       receive_static<MessageMem>);
    vcpu->memregion.add(this, receive_static<MessageMemRegion>);
    vcpu->bus_lapic.add(this, receive_static<LapicEvent>);

  }
};


PARAM_HANDLER(lapic,
	      "lapic:inital_apic_id - provide an x2APIC for the last VCPU",
	      "Example: 'lapic:2'",
	      "If no inital_apic_id is given the lapic number is used.")
{
  if (!mb.last_vcpu) Logging::panic("no VCPU for this APIC");

  // allocate a timer
  MessageTimer msg0;
  if (!mb.bus_timer.send(msg0))
    Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);

  static unsigned lapic_count;
  new Lapic(mb, mb.last_vcpu, ~argv[0] ? argv[0]: lapic_count, msg0.nr);
  lapic_count++;
}



#else
REGSET(Lapic,
       REG_RW(_ID,            0x02,          0, 0xff000000,)
       REG_RO(_VERSION,       0x03, 0x01050014)
       REG_RW(_TPR,           0x08,          0, 0xff,)
       REG_RW(_LDR,           0x0d,          0, 0xff000000,)
       REG_RW(_DFR,           0x0e, 0xffffffff, 0xf0000000,)
       REG_RW(_SVR,           0x0f, 0x000000ff, 0x11ff,     update_irqs();)
       REG_RW(_ESR,           0x28,          0, 0xffffffff, _ESR = Cpu::xchg(&_esr_shadow, 0U); return !value; )
       REG_RW(_ICR,           0x30,          0, 0x000ccfff, if (!send_ipi(_ICR, _ICR1)) COUNTER_INC("IPI missed");)
       REG_RW(_ICR1,          0x31,          0, 0xff000000,)
       REG_RW(_TIMER,         0x32, 0x00010000, 0x310ff, )
       REG_RW(_TERM,          0x33, 0x00010000, 0x117ff, )
       REG_RW(_PERF,          0x34, 0x00010000, 0x117ff, )
       REG_RW(_LINT0,         0x35, 0x00010000, 0x1b7ff, )
       REG_RW(_LINT1,         0x36, 0x00010000, 0x1b7ff, )
       REG_RW(_ERROR,         0x37, 0x00010000, 0x110ff, )
       REG_RW(_ICT,           0x38,          0, ~0u,
	      COUNTER_INC("lapic ict");
	      _timer_start = _mb.clock()->time();
	      update_timer(_timer_start); )
       REG_RW(_DCR,           0x3e,          0, 0xb
,
	      {
		timevalue now = _mb.clock()->time();
		unsigned  done = _ICT - get_ccr(now);
		_timer_dcr_shift = _timer_clock_shift + ((((_DCR & 0x3) | ((_DCR >> 1) & 4)) + 1) & 7);

		/**
		 * Move timer_start in the past, which what would be
		 * already done on this period with the current speed.
		 */
		_timer_start     = now - (done << _timer_dcr_shift);
		update_timer(now);
	      }
	      ))
#endif
