/**
 * Route messages between HostIrq and Irqlines.
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
 * Route IRQ messages from host to guest.
 *
 * State: testing
 * Missing: level triggered IRQs
 */
class IRQRouting : public StaticReceiver<IRQRouting>
{
  DBus<MessageIrq> &_bus_irqlines;
  unsigned _host_irq;
  unsigned _guest_irq;


  const char* debug_getname() { return "IRQRouting"; }
  void debug_dump() {
    Device::debug_dump();
    Logging::printf(" (%x) -> (%x)", _host_irq, _guest_irq);
  };


 public:
  bool  receive(MessageIrq &msg)
  {
    if (msg.line == _host_irq)
      {
	MessageIrq msg2(msg.type, _guest_irq);
	return _bus_irqlines.send(msg2);
      };
    return false;
  }

  IRQRouting(DBus<MessageIrq> &bus_irqlines, unsigned host_irq, unsigned guest_irq)
    : _bus_irqlines(bus_irqlines), _host_irq(host_irq), _guest_irq(guest_irq)
  {}
};


PARAM(hostirq,
      {
	mb.bus_hostirq.add(new IRQRouting(mb.bus_irqlines, argv[0], argv[1]), &IRQRouting::receive_static<MessageIrq>);
	MessageHostOp msg(MessageHostOp::OP_ATTACH_HOSTIRQ, argv[0]);
	if (!mb.bus_hostop.send(msg))
	  Logging::panic("%s failed to attach hostirq %lx\n", __PRETTY_FUNCTION__, msg.value);
      },
      "hostirq:hostirq:irq - add an IRQ redirection from host vectors to guest IRQs.",
      "Example: 'hostirq:0x20,0x00'.");
