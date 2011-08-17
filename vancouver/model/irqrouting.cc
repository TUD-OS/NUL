/** @file
 * Route messages between HostIrq and Irqlines.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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
 * Route IRQ messages from host to guest.
 *
 * State: testing
 * Missing: level triggered IRQs
 */
class IRQRouting : public StaticReceiver<IRQRouting>
{
  Motherboard &_mb;
  unsigned _host_irq;
  unsigned _guest_irq;
  unsigned _msi_vector;


 public:
  bool  receive(MessageIrq &msg)
  {
    if (msg.line == _host_irq) {

      MessageMem msg1(false, MessageMem::MSI_ADDRESS, &_msi_vector);
      MessageIrqLines msg2(msg.type, _guest_irq);
      if (_msi_vector >= 0x10 && _msi_vector < 0x100) _mb.bus_mem.send(msg1);
      return _mb.bus_irqlines.send(msg2);
    }
    return false;
  }

  IRQRouting(Motherboard &mb, unsigned host_irq, unsigned guest_irq, unsigned msi_vector)
    : _mb(mb), _host_irq(host_irq), _guest_irq(guest_irq), _msi_vector(msi_vector)
  {}
};


PARAM_HANDLER(hostirq,
	      "hostirq:hostgsi,irq,msi - add an IRQ redirection from host vectors to guest IRQs.",
	      "Example: 'hostirq:0x08,0x00,0x50'.")
{
  mb.bus_hostirq.add(new IRQRouting(mb, argv[0], argv[1], argv[2]), IRQRouting::receive_static<MessageIrq>);
  MessageHostOp msg(MessageHostOp::OP_ATTACH_IRQ, argv[0]);
  if (!mb.bus_hostop.send(msg))
    Logging::panic("%s failed to attach hostirq %lx\n", __PRETTY_FUNCTION__, msg.value);
}

