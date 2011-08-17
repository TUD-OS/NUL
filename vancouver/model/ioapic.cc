/** @file
 * I/OxAPIC model.
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

/**
 * I/OxAPIC model.
 *
 * State: testing
 * Features: MSI generation, level+notify, PAR, EOI
 * Difference: no APIC bus
 * Documentation: Intel ICH4.
 */
class IOApic : public DiscoveryHelper<IOApic>, public StaticReceiver<IOApic> {
public:
  enum {
    IOAPIC_BASE = 0xfec00000,
    OFFSET_INDEX= 0x00,
    OFFSET_DATA = 0x10,
    OFFSET_PAR  = 0x20,
    OFFSET_EOI  = 0x40,
    PINS        = 24,
    EXTINT_PIN  = 0,
    NMI_PIN     = 23,
  };
  Motherboard &_mb;

private:
  unsigned _base;
  unsigned _gsibase;

  unsigned char _index;
  unsigned _id;
  unsigned _redir [PINS*2];
  bool     _rirr  [PINS];
  bool     _ds    [PINS];
  bool     _notify[PINS];

  /**
   * Route IRQs and return a pin to a GSI number.
   */
  unsigned irq_routing(unsigned gsi) {
    // we do the IRQ line routing here and switch GSI2 and 0
    if (gsi == 2 || !gsi) gsi = 2 - gsi;
    return gsi - _gsibase;
  }

  /**
   * Return an GSI from a pin.
   */
  unsigned reverse_routing(unsigned pin) {
    pin += _gsibase;
    if (pin == 2 || !pin) pin = 2 - pin;
    return pin;
  }


  /**
   * Read the data register.
   */
  void read_data(unsigned &value) {
    if (in_range(_index, 0x10, 0x10 + PINS*2)) {
      value = _redir[_index - 0x10];
      if (_ds  [(_index - 0x10) / 2]) value |= 1 << 12;
      if (_rirr[(_index - 0x10) / 2]) value |= 1 << 14;
    }
    else if (_index == 0)
      value = _id;
    else if (_index == 1)
      value = 0x00008020 | ((PINS - 1) << 16);
  }


  /**
   * Write to the data register.
   */
  void write_data(unsigned value) {
    if (in_range(_index, 0x10, 0x3f)) {
      unsigned mask = (_index & 1) ? 0xffff0000 : 0x1afff;
      _redir[_index - 0x10] = value & mask;
      unsigned pin = (_index - 0x10) / 2;

      // if edge: clear ds bit
      _ds[pin] = _ds[pin] && _redir[pin * 2] & MessageApic::ICR_LEVEL;

      // unmasked - retrigger and/or notify
      if (~_redir[pin * 2] & 0x10000) {
	if (_ds[pin]) pin_assert(pin, MessageIrq::ASSERT_NOTIFY);
	else {
	  // unmasked an edge triggered IRQ? -> notify
	  _notify[pin] = true;
	  notify(pin);
	}
      }
    }
    else if (_index == 0)
      /**
       * We allow to write every bit, as we never actually use the value.
       */
      _id = value;
  }

  void notify(unsigned pin) {
    if (_notify[pin]) {
      _notify[pin] = false;
      unsigned gsi = reverse_routing(pin);
      MessageIrqNotify msg(gsi & ~7, 1  << (gsi & 7));
      _mb.bus_irqnotify.send(msg);
    }
  }

  /**
   * Assert a pin on this IO/APIC.
   */
  bool pin_assert(unsigned pin, MessageIrq::Type type) {
    if (pin >= PINS) return false;
    if (type == MessageIrq::DEASSERT_IRQ) _ds[pin] = false;
    else {
      // have we already send the message
      if (_rirr[pin]) return true;
      unsigned dst   = _redir[2*pin+1];
      unsigned value = _redir[2*pin];
      bool level     = value & 0x8000;
      _notify[pin] = type == MessageIrq::ASSERT_NOTIFY;
      if (value & 0x10000) {
	if (level) _ds[pin] = true;
      } else {

	_rirr[pin] = level;
	_ds[pin]   = false;
	unsigned long phys = MessageMem::MSI_ADDRESS | (dst >> 12) & 0xffff0;
	if (value & MessageApic::ICR_DM) phys |= MessageMem::MSI_DM;
	if ((value & 0x700) == 0x100)    phys |= MessageMem::MSI_RH;
	if (_rirr[pin])                 value |= 1 << 14;

	MessageMem mem(false, phys, &value);
	_mb.bus_mem.send(mem);
	if (!level) notify(pin);
      }
    }
    return true;
  }


  /**
   * EOI a vector.
   */
  void eoi(unsigned char vector) {
    for (unsigned i=0; i < PINS; i++)
      if ((_redir[i*2] & 0xff) == vector && _rirr[i]) {
	_rirr[i] = false;
	notify(i);
      }
  }


  /**
   * Reset the registers.
   */
  void reset() {
    for (unsigned i=0; i < PINS; i++) {
      _redir[2*i]   = 0x10000;
      _redir[2*i+1] = 0;
      _notify[i]    = false;
      _ds[i]        = false;
      _rirr[i]      = false;
    }
    // enable virtual wire mode?
    if (!_gsibase) {
      _redir[2*0]     = 0x10700;
      _redir[2*23]    = 0x10400;
    }
    _id = 0;
    _index = 0;
  }

public:
  bool  receive(MessageMem &msg) {
    if (!in_range(msg.phys, _base, 0x100) &&
	// all IOApics should get the broadcast EOI from the LAPIC
	msg.phys != MessageApic::IOAPIC_EOI) return false;
    switch (msg.phys & 0xff) {
    case OFFSET_INDEX:
      if (msg.read)  *msg.ptr = _index; else  _index = *msg.ptr;
      return true;
    case OFFSET_DATA:
      if (msg.read) read_data(*msg.ptr); else write_data(*msg.ptr);
      return true;
    case OFFSET_PAR:
      if (msg.read) break;
      pin_assert(*msg.ptr, MessageIrq::ASSERT_IRQ);
      return true;
    case OFFSET_EOI:
      if (msg.read) break;
      eoi(*msg.ptr);
      return true;
    }
    return false;
  }


  bool  receive(MessageIrq &msg) {
    if (!in_range(msg.line, _gsibase, PINS)) return false;
    COUNTER_INC("GSI");
    pin_assert(irq_routing(msg.line), msg.type);
    return true;
  }


  bool  receive(MessageLegacy &msg) {
    if (!_gsibase) {
      if (msg.type == MessageLegacy::INTR)   return pin_assert(EXTINT_PIN,  MessageIrq::ASSERT_IRQ);
      if (msg.type == MessageLegacy::NMI)    return pin_assert(NMI_PIN,     MessageIrq::ASSERT_IRQ);
    }
    if (msg.type == MessageLegacy::RESET)  { reset(); return true; }
    return false;
  }

  void discovery() {

    unsigned length = discovery_length("APIC", 44);

    if (!_gsibase) {
      // override IRQ 0->2
      discovery_write_dw("APIC", length + 0, 0x00000a02, 4);
      discovery_write_dw("APIC", length + 4,          2, 4);
      discovery_write_dw("APIC", length + 8,          0, 2);
      // NMI connection is edge high
      discovery_write_dw("APIC", length + 10,  0x0803, 4);
      discovery_write_dw("APIC", length + 14, NMI_PIN, 4);
      length += 18;

    }

    // the I/O APIC structure
    discovery_write_dw("APIC", length + 0,   0x0c01, 4);
    discovery_write_dw("APIC", length + 4,    _base, 4);
    discovery_write_dw("APIC", length + 8, _gsibase, 4);
  }



  IOApic(Motherboard &mb, unsigned long base, unsigned gsibase) : _mb(mb), _base(base), _gsibase(gsibase)
  {
    reset();
    _mb.bus_mem.add(this,       receive_static<MessageMem>);
    _mb.bus_irqlines.add(this,  receive_static<MessageIrqLines>);
    _mb.bus_legacy.add(this,    receive_static<MessageLegacy>);
    _mb.bus_discovery.add(this, discover);
  };
};


PARAM_HANDLER(ioapic,
	      "ioapic - create an ioapic.",
	      "The GSIs are automatically distributed, so that the first IOAPIC gets GSI0-23, the next 24-47...")
{
  static unsigned ioapic_count;
  new IOApic(mb, IOApic::IOAPIC_BASE + 0x1000 * ioapic_count, IOApic::PINS*ioapic_count);
  ioapic_count++;
}
