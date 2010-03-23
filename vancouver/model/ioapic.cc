/**
 * I/OxAPIC model.
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

/**
 * I/OxAPIC model.
 *
 * State: unstable
 * Features: MSI generation, level+notify, PAR, EOI
 * Difference: no APIC bus
 * Documentation: Intel ICH4.
 */
class IOApic : public StaticReceiver<IOApic> {
public:
  enum {
    IOAPIC_BASE = 0xfec00000,
    OFFSET_INDEX= 0x00,
    OFFSET_DATA = 0x10,
    OFFSET_PAR  = 0x20,
    OFFSET_EOI  = 0x40,
    PINS        = 24,
  };
private:
  DBus<MessageMem>       &_bus_mem;
  DBus<MessageIrqNotify> &_bus_notify;
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

      // delivery pending?
      if (_ds[pin]) {
	// if edge: clear ds bit
	_ds[pin] = _redir[pin * 2] & MessageApic::ICR_LEVEL;

	// if ds, level and unmasked - retrigger
	if (_ds[pin] && ~_redir[pin * 2] & 0x10000)
	  pin_assert(pin, _notify[pin] ? MessageIrq::ASSERT_NOTIFY : MessageIrq::ASSERT_IRQ);
      }
    }
    else if (_index == 0)
      /**
       * We allow to write every bit, as we never actually use the value.
       */
      _id = value;
  }


  /**
   * Assert a pin on this IO/APIC.
   */
  void pin_assert(unsigned pin, MessageIrq::Type type) {
    if (pin >= PINS) return;
    if (MessageIrq::DEASSERT_IRQ) _ds[pin] = false;
    else {
      // have we already send the message
      if (_rirr[pin]) return;
      unsigned dst   = _redir[2*pin+1];
      unsigned value = _redir[2*pin];
      bool level     = value & 0x8000;
      _notify[pin] = type == MessageIrq::ASSERT_NOTIFY;
      if (value & 0x10000) {
	if (level)_ds[pin] = true;
	return;
      }
      _rirr[pin]= level;

      unsigned long phys = MessageMem::MSI_ADDRESS | (dst >> 12) & 0xffff0;
      if (value & MessageApic::ICR_DM) phys |= MessageMem::MSI_DM;
      if ((value & 0x700) == 0x100)    phys |= MessageMem::MSI_RH;
      MessageMem mem(false, phys, &value);
      _bus_mem.send(mem);
    }
  }


  /**
   * EOI a vector.
   */
  void eoi(unsigned char vector) {
    for (unsigned i=0; i < PINS; i++)
      if ((_redir[i*2] & 0xff) == vector && _rirr[i]) {
	_rirr[i] = false;
	if (_notify[i]) {
	  unsigned gsi = reverse_routing(i);
	  MessageIrqNotify msg(gsi & ~0x7, 1  << (gsi & 7));
	  _bus_notify.send(msg);
	}
      }
  }


  /**
   * Reset the registers.
   */
  void reset() {
    for (unsigned i=0; i < PINS; i++) {
      _redir[2*i]   = 0x10000;
      _redir[2*i+1] = 0;
      _notify[i] = false;
      _ds[i] = false;
      _rirr[i] = false;
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
    pin_assert(irq_routing(msg.line), msg.type);
    return true;
  }


  bool  receive(MessageLegacy &msg) {
    if (msg.type != MessageLegacy::RESET)  return false;
    reset();
    return true;
  }


  IOApic(DBus<MessageMem> &bus_mem, DBus<MessageIrqNotify> &bus_notify, unsigned long base, unsigned gsibase) 
    : _bus_mem(bus_mem), _bus_notify(bus_notify), _base(base), _gsibase(gsibase) {
    reset();
  };
};


PARAM(ioapic,
      static unsigned ioapic_count;

      Device *dev = new IOApic(mb.bus_mem, mb.bus_irqnotify, IOApic::IOAPIC_BASE + 0x1000 * ioapic_count, IOApic::PINS*ioapic_count);
      mb.bus_mem.add(dev, IOApic::receive_static<MessageMem>);
      mb.bus_legacy.add(dev, IOApic::receive_static<MessageLegacy>);

      ioapic_count++;
      ,
      "ioapic - create an ioapic.",
      "The GSIs are automatically distributed, so that the first IOAPIC gets GSI0-23, the next 24-47...");
