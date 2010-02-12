/**
 * HostHpet driver.
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

/**
 * Use the HPET as timer backend.
 *
 * State:    unstable
 * Features: periodic timer, support different timers, one-shot, HPET ACPI table, MSI
 */
class HostHpet : public StaticReceiver<HostHpet>
{
  DBus<MessageTimeout> &_bus_timeout;
  Clock *_clock;
  struct HostHpetTimer {
    volatile unsigned config;
    volatile unsigned int_route;
    volatile unsigned comp[2];
    volatile unsigned msi[2];
    unsigned res[2];
  };
  struct HostHpetRegister {
    volatile unsigned cap;
    volatile unsigned period;
    unsigned res0[2];
    volatile unsigned config;
    unsigned res1[3];
    volatile unsigned isr;
    unsigned res2[51];
    union {
      volatile unsigned  counter[2];
      volatile unsigned long long main;
    };
    unsigned res3[2];
    struct HostHpetTimer timer[24];
  } *_regs;
  unsigned  _timer;
  struct HostHpetTimer *_timerreg;
  unsigned  _irq;
  bool      _edge;
  timevalue _freq;

  // debug
  timevalue _lasttimeout;
  unsigned  _lastdelta;

  const char *debug_getname() {  return "HostHPET"; }
public:
  unsigned irq() { return _irq; }


  bool  receive(MessageIrq &msg)
  {
    timevalue now = _clock->time();
    COUNTER_SET("last_to", _lasttimeout);
    if (msg.line == _irq && msg.type == MessageIrq::ASSERT_IRQ || (now - 1000000) > _lasttimeout)
      {
	if ((now - 1000000) > _lasttimeout) 	COUNTER_INC("HPET lostlong");
	if ((now > _lasttimeout + 100000000ULL) && (msg.line == _irq))
	  {
	    Logging::printf("HPET halted %lld cycles lastdelta %x %llx\n", now - _lasttimeout, _lastdelta, now);
	    COUNTER_INC("HPET halt");
	  }
	COUNTER_INC("HPET irq");
	_lasttimeout = ~0ull - 100000000ULL;

	// reset the irq output
	if (!_edge) _regs->isr = 1 << _timer;
	MessageTimeout msg2(MessageTimeout::HOST_TIMEOUT);
	_bus_timeout.send(msg2);
	return true;
      }
    return false;
  }


  bool  receive(MessageTimer &msg)
  {
    if (msg.nr != MessageTimeout::HOST_TIMEOUT) return false;
    if (msg.abstime == ~0ull) return false;

    _lasttimeout = msg.abstime;
    COUNTER_INC("HPET reprogram");

    unsigned delta = _clock->delta(msg.abstime, _freq);
    _lastdelta = delta;
    unsigned oldvalue = _regs->counter[0];
    _timerreg->comp[0] = oldvalue + delta;
    // we read them back to avoid PCI posting problems on ATI chipsets
    (void) _timerreg->comp[0];
    unsigned newvalue = _regs->counter[0];
    if (((newvalue - oldvalue) >= delta) || (msg.abstime <= _clock->time()))
      {
	COUNTER_INC("HPET lost");
	COUNTER_SET("HPET da2", delta);
	COUNTER_SET("HPET ov2", oldvalue);
	COUNTER_SET("HPET nv2", newvalue);
	unsigned v1 = _regs->counter[0];
	unsigned v2 = _regs->counter[0];
	COUNTER_SET("HPET v", v2 - v1);
	MessageTimeout msg2(MessageTimeout::HOST_TIMEOUT);
	_bus_timeout.send(msg2);

      }
    return true;
  }


  HostHpet(DBus<MessageTimeout> &bus_timeout, DBus<MessageHostOp> &bus_hostop, Clock *clock, void *iomem, unsigned timer, unsigned theirq, bool edge)
    : _bus_timeout(bus_timeout), _clock(clock), _regs(reinterpret_cast<HostHpetRegister *>(iomem)), _timer(timer),
      _timerreg(_regs->timer + _timer), _irq(theirq), _edge(edge)
  {
    Logging::printf("HostHpet: cap %x config %x period %d ", _regs->cap, _regs->config, _regs->period);
    if (_regs->period > 0x05f5e100 || !_regs->period) Logging::panic("Invalid HPET period");

    _freq = 1000000000000000ull;
    Math::div64(_freq, _regs->period);
    Logging::printf(" freq %lld\n", _freq);

    unsigned num_timer = ((_regs->cap & 0x1f00) >> 8) + 1;
    if (num_timer < _timer)  Logging::panic("Timer %x not supported", _timer);
    for (unsigned i=0; i < num_timer; i++)
      Logging::printf("\tHpetTimer[%d]: config %x int %x\n", i, _regs->timer[i].config, _regs->timer[i].int_route);

    // get the IRQ number
    bool legacy = false;
    if (_irq == ~0u)
      {
	if (_regs->cap & 0x8000 && _timer < 2)
	  {
	    legacy =  true;
	    if (_timer == 0) _irq = 2;
	    else             _irq = 8;
	  }
	else
	  {
	    // MSI?
	    MessageHostOp msg1(MessageHostOp::OP_GET_MSIVECTOR, 0);
	    if ((_timerreg->config & (1<<15)) &&  bus_hostop.send(msg1))
	      {
		// enable MSI
		_irq = msg1.value;
		_timerreg->msi[0] = MSI_VALUE + _irq;
		_timerreg->msi[1] = MSI_ADDRESS;
		_timerreg->config |= 1<<14;
	      }
	    else
	      {
		if (!_timerreg->int_route)  Logging::panic("No IRQ routing possible for timer %x", _timer);
		_irq = Cpu::bsf(_timerreg->int_route);
	      }
	  }
      }
    else if (~_timerreg->int_route & (1 << _irq))  Logging::panic("IRQ routing to GSI %x impossible for timer %x", _irq, _timer);
    Logging::printf("HostHpet: using counter %x GSI 0x%02x (%s%s)\n", _timer, _irq, _edge ? "edge" : "level", legacy ? ", legacy" : "");

    // enable timer in non-periodic 32bit mode
    _timerreg->config = (_timerreg->config & ~0xa) | ((_irq & 0x1f) << 9) | 0x104 | (_edge ? 0 : 2);

    // enable main counter and legacy mode
    _regs->config |= legacy ? 3 : 1;

    // ack interrupts
    if (!_edge) _regs->isr = ~0u;
  }
};

PARAM(hosthpet,
      {
	unsigned long address = argv[0];

	// get address from HPET ACPI table
	if (address == ~0ul)
	  {
	    MessageAcpi msg0("HPET");
	    if (!mb.bus_acpi.send(msg0) || !msg0.table)  { Logging::printf("Warning: no HPET ACPI table -> no HostHpet\n"); return; }

	    struct HpetAcpiTable
	    {
	      char res[40];
	      unsigned char gas[4];
	      unsigned long address[2];
	    };
	    HpetAcpiTable *table = reinterpret_cast<HpetAcpiTable *>(msg0.table);
	    if (table->gas[0])     Logging::panic("HPET access must be MMIO but is %d", table->gas[0]);
	    if (table->address[1]) Logging::panic("HPET must be below 4G");
	    address = table->address[0];
	  }

	// alloc MMIO region
	MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOMEM, address, 1024);
	if (!mb.bus_hostop.send(msg1) || !msg1.ptr)  Logging::panic("%s failed to allocate iomem %lx+0x400\n", __PRETTY_FUNCTION__, address);

	// check whether this looks like an HPET
	unsigned cap = *reinterpret_cast<volatile unsigned *>(msg1.ptr);
	if (cap == ~0u || !(cap & 0xff))
	  {
	    Logging::printf("This is not an HPET at %lx value %x\n", address, cap);
	    return;
	  }


	// create device
	HostHpet *dev = new HostHpet(mb.bus_timeout, mb.bus_hostop, mb.clock(), msg1.ptr, ~argv[1] ? argv[1] : 0, argv[2], argv[3]);
	mb.bus_hostirq.add(dev, &HostHpet::receive_static<MessageIrq>);
	mb.bus_timer.add(dev, &HostHpet::receive_static<MessageTimer>);

	// allocate hostirq
	MessageHostOp msg2(MessageHostOp::OP_ATTACH_HOSTIRQ, dev->irq());
	if (!(msg2.value == ~0U || mb.bus_hostop.send(msg2)))
	  Logging::panic("%s failed to attach hostirq %lx\n", __PRETTY_FUNCTION__, msg2.value);
      },
      "hosthpet:address,timer=0,irq=~0u,edge=1 - use the host HPET as timer.",
      "If no address is given, the ACPI HPET table is used.",
      "If no irq is given, either the legacy or the lowest possible IRQ is used.",
      "Example: 'hosthpet:0xfed00000,1' - for the second timer of the hpet at 0xfed00000.");


#include "host/hostpci.h"
PARAM(quirk_hpet_ich,
      HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
      unsigned address = pci.conf_read(0xf8, 0xf0);

      if (!address) return;

      MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOMEM, address & ~0x3fff, 0x4000);
      if (!mb.bus_hostop.send(msg1) || !msg1.ptr)  Logging::panic("%s failed to allocate iomem %x+0x4000\n", __PRETTY_FUNCTION__, address);

      volatile unsigned *reg = reinterpret_cast<volatile unsigned *>(msg1.ptr + 0x3404);
      Logging::printf("HPET try enable on ICH addr %x val %x\n", address, *reg);

      // enable hpet decode
      *reg = *reg | 0x80;
      Logging::printf("ICH HPET force %s %x\n", *reg & 0x80 ? "enabled" : "failed", *reg);
      ,
      "quirk_hpet_ich - force enable the HPET on an ICH chipset.",
      "Pleas bote, that this does not check whether this is done on the right chipset - use it on your own risk!");
