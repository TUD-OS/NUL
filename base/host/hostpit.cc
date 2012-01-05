/** @file
 * HostPit driver.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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

/**
 * Program the PIT with a given frequency.
 *
 * State: stable
 */
class HostPit : public StaticReceiver<HostPit>
{
  #include "host/simplehwioout.h"
  DBus<MessageTimeout> &_bus_timeout;
  Clock *_clock;
  static const unsigned long long FREQ = 1193180;
  unsigned _period;
  unsigned _iobase;
  unsigned _irq;


 public:
  bool  receive(MessageIrq &msg)
  {
    if (msg.line == _irq && msg.type == MessageIrq::ASSERT_IRQ) {
      MessageTimeout msg2(0, _clock->time());
	_bus_timeout.send(msg2);
	return true;
      }
    return false;
  }


  HostPit(DBus<MessageHwIOOut> &bus_hwioout, DBus<MessageTimeout> &bus_timeout, Clock *clock, unsigned period, unsigned iobase, unsigned irq)
    :  _bus_hwioout(bus_hwioout), _bus_timeout(bus_timeout), _clock(clock), _period(period), _iobase(iobase), _irq(irq)
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


PARAM_HANDLER(hostpit,
	      "hostpit:period,hostiobase,hostirq - use the host PIT as timer, the period is given in microseconds.",
	      "Example: 'hostpit:4000,0x40,2'.")
{
  MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, (argv[1] << 8) |  2);
  if (!mb.bus_hostop.send(msg1))
    Logging::panic("%s failed to allocate ports %lx+4\n", __PRETTY_FUNCTION__, argv[1]);

  HostPit *dev = new HostPit(mb.bus_hwioout, mb.bus_timeout, mb.clock(), argv[0], argv[1], argv[2]);
  mb.bus_hostirq.add(dev, HostPit::receive_static<MessageIrq>);

  MessageHostOp msg2 = MessageHostOp::attach_irq(argv[2], ~0U, false, "hpit");
  if (!(msg2.value == ~0U || mb.bus_hostop.send(msg2)))
    Logging::panic("%s failed to attach hostirq %lx\n", __PRETTY_FUNCTION__, argv[2]);
}
