/** @file
 * HostHpet driver.
 *
 * Copyright (C) 2009-2010, Bernhard Kauer <bk@vmmon.org>
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

#include <nul/motherboard.h>
#include <host/hpet.h>

/**
 * Use the HPET as timer backend.
 *
 * State: testing
 * Features: periodic timer, support different timers, one-shot, HPET ACPI table, MSI
 */
class HostHpet : public BasicHpet, public StaticReceiver<HostHpet>
{
  DBus<MessageTimeout> &_bus_timeout;
  Clock *_clock;
  struct HostHpetRegister *_regs;
  unsigned  _isrclear;
  struct HostHpetTimer *_timerreg;
  unsigned  _irq;
  timevalue _freq;
  unsigned  _mindelta;
public:


  bool  receive(MessageIrq &msg)
  {
    if (msg.line == _irq && msg.type == MessageIrq::ASSERT_IRQ)
      {
	// ack irq
	if (_isrclear) _regs->isr = _isrclear;
	MessageTimeout msg2(0, _clock->time());
	_bus_timeout.send(msg2);
	return true;
      }
    return false;
  }


  bool  receive(MessageTimer &msg)
  {
    if ((msg.abstime == ~0ull) or (msg.nr != 0)) return false;

    // delta is truncated, it should be rounded "upwards" :-)
    unsigned delta = _clock->delta(msg.abstime, _freq);
    if (delta < _mindelta) delta = _mindelta;
    unsigned newvalue = _regs->counter[0] + delta;

    // write new comparator value
    _timerreg->comp[0] = newvalue;

    /**
     * Use the same heuristic as in Linux 2.6.37 to avoid various
     * corner cases. See 995bd3bb5c78f3ff71339803c0b8337ed36d64fb for
     * a detailed explanation.
     */
    if (static_cast<signed int>(newvalue - _regs->counter[0]) <= 8) {
      COUNTER_INC("HPET lost");
      MessageTimeout msg2(0, _clock->time());
      _bus_timeout.send(msg2);
    }
    return true;
  }


  HostHpet(DBus<MessageTimeout> &bus_timeout, DBus<MessageHostOp> &bus_hostop, Clock *clock, void *iomem, unsigned timer, unsigned theirq, bool level, unsigned long maxfreq)
    : _bus_timeout(bus_timeout), _clock(clock), _regs(reinterpret_cast<HostHpetRegister *>(iomem)), _timerreg(_regs->timer + timer), _irq(theirq)
  {
    _freq = 1000000000000000ull;
    Math::div64(_freq, _regs->period);
    _mindelta = Math::muldiv128(maxfreq, 1, _freq);

    // get the IRQ number
    bool legacy = false;
    if (_irq == ~0u) {
      // legacy supported -> enable it
      if (_regs->cap & 0x8000 && timer < 2) {
	legacy =  true;
	_irq = timer ? 8 : 2;
      }
      else {
	// MSI?
	MessageHostOp msg1 = MessageHostOp::attach_msi(~0U, false, 0UL, "hhpet msi");
	if ((_timerreg->config & (1<<15)) &&  bus_hostop.send(msg1)) {
	  _irq = msg1.msi_gsi;
	  _timerreg->msi[0] = msg1.msi_value;
	  _timerreg->msi[1] = msg1.msi_address;
	  _timerreg->config |= 1<<14;
	  level = false;
	}
	else {
	  assert(_timerreg->int_route);
	  _irq = Cpu::bsf(_timerreg->int_route);
	}
      }
    }
    else
      assert(_timerreg->int_route & (1 << _irq));

    // the HV assumes that GSI below 16 are edge triggered
    if (_irq < 16) level = false;
    _isrclear = level ? (1 << timer) : 0;

    // enable timer in non-periodic 32bit mode
    _timerreg->config = (_timerreg->config & ~0xa) | ((_irq & 0x1f) << 9) | 0x104 | (level ? 2 : 0);

    // enable main counter and legacy mode
    _regs->config |= legacy ? 3 : 1;

    // clear pending IRQs
    _regs->isr = _isrclear;

    Logging::printf("HostHpet: using counter %x GSI 0x%02x (%s%s)\n", timer, _irq, level ? "level" : "edge", legacy ? ", legacy" : "");

    MessageHostOp msg2 = MessageHostOp::attach_irq(_irq, ~0U, false, "hhpet");
    if (!bus_hostop.send(msg2))
      Logging::panic("%s failed to attach hostirq %lx\n", __PRETTY_FUNCTION__, msg2.value);

  }
};

PARAM_HANDLER(hosthpet,
	      "hosthpet:timer=0,address,irq=~0u,level=1,maxfreq=10000 - use the host HPET as timer.",
	      "If no address is given, the ACPI HPET table or 0xfed00000 is used.",
	      "If no irq is given, either the legacy or the lowest possible IRQ is used.",
	      "The maxfreq parameter defines the maximum IRQ rate and therefore accuracy of the device.",
	      "Example: 'hosthpet:1,0xfed00000' - for the second timer of the hpet at 0xfed00000.")
{
  unsigned timer = ~argv[0] ? argv[0] : 0;
  unsigned long address = argv[1];
  if (address == ~0U) address = BasicHpet::get_hpet_address(mb.bus_acpi);
  unsigned irq = argv[2];

  MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOMEM, address, 1024);
  if (!mb.bus_hostop.send(msg1) || !msg1.ptr)  Logging::panic("%s failed to allocate iomem %lx+0x400\n", __PRETTY_FUNCTION__, address);

  if (not BasicHpet::check_hpet_present(msg1.ptr, timer, irq)) {
    Logging::printf("This is not an HPET timer at %lx timer %x\n", address, timer);
    return;
  }

  // create device
  HostHpet *dev = new HostHpet(mb.bus_timeout, mb.bus_hostop, mb.clock(), msg1.ptr, timer, irq, argv[3], argv[4]);
  mb.bus_hostirq.add(dev, HostHpet::receive_static<MessageIrq>);
  mb.bus_timer.add(dev,   HostHpet::receive_static<MessageTimer>);

}


#include "host/hostpci.h"
PARAM_HANDLER(quirk_hpet_ich,
      "quirk_hpet_ich - force enable the HPET on an ICH chipset.",
      "Please note that this does not check whether this is done on the right chipset - use it on your own risk!")
{
  HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
  unsigned address = pci.conf_read(0xf8, 0x3c);

  if (!address) return;

  MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOMEM, address & ~0x3fff, 0x4000);
  if (!mb.bus_hostop.send(msg1) || !msg1.ptr)  Logging::panic("%s failed to allocate iomem %x+0x4000\n", __PRETTY_FUNCTION__, address);

  volatile unsigned *reg = reinterpret_cast<volatile unsigned *>(msg1.ptr + 0x3404);
  Logging::printf("HPET try enable on ICH addr %x val %x\n", address, *reg);

  // enable hpet decode
  *reg = *reg | 0x80;
  Logging::printf("ICH HPET force %s %x\n", *reg & 0x80 ? "enabled" : "failed", *reg);
}
