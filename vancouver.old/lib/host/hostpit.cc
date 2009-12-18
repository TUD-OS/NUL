/**
 * HostPit driver.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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
 * Program the PIT with a given frequency.
 *
 * State: stable
 */
class HostPit : public StaticReceiver<HostPit>
{
  #include "host/simplehwioout.h"
  static const long long FREQ = 1193180;
  unsigned _period;
  unsigned _iobase;
  unsigned _irq;
  const char *debug_getname() {  return "HostPIT"; }
  void debug_dump() {  
    Device::debug_dump();
    Logging::printf(" iobase %x period %x irq %x", _iobase, _period, _irq);
  };


 public:
  bool  receive(MessageIrq &msg)
  {
    if (msg.line == _irq && msg.type == MessageIrq::ASSERT_IRQ)
      {
	// 
	return true;
      }
    return false;
  }


  HostPit(DBus<MessageIOOut> &bus_hwioout, unsigned period, unsigned iobase, unsigned irq) 
   :  _bus_hwioout(bus_hwioout), _period(period), _iobase(iobase), _irq(irq)
  {
    unsigned long long value = FREQ*period;
    Math::div64(value, 1000000);
    if (!value || (value > 65535))
      Logging::panic("%s unsupported period %x -> value %llx\n",__PRETTY_FUNCTION__, period, value);
    outb(0x34, iobase + 3);
    outb(value, iobase);
    outb(value>>8, iobase);
  }
};


PARAM(hostpit,
      {
	MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, (argv[1] << 8) |  2);
	if (!mb.bus_hostop.send(msg1))
	  Logging::panic("%s failed to allocate ports %lx+4\n", __PRETTY_FUNCTION__, argv[1]);

	Device *dev = new HostPit(mb.bus_hwioout, argv[0], argv[1], argv[2]);
	mb.bus_hostirq.add(dev, &HostPit::receive_static<MessageIrq>);

	MessageHostOp msg2(MessageHostOp::OP_ATTACH_HOSTIRQ, argv[2]);
	if (!(msg2.value == ~0U || mb.bus_hostop.send(msg2)))
	  Logging::panic("%s failed to attach hostirq %lx\n", __PRETTY_FUNCTION__, argv[2]);
      },
      "hostpit:period,hostiobase,hostirq - use the host PIT as timer, the period is given in microseconds.",
      "Example: 'hostpit:10000,0x40,0x20'.");
