/** @file
 * MSI support.
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
 * Forward Message Signaled IRQs to the local APICs.
 *
 * State: testing
 * Features: LowestPrio: RoundRobin, 16bit dest
 */
class Msi  : public StaticReceiver<Msi> {
  DBus<MessageApic>  & _bus_apic;
  unsigned  _lowest_rr;

public:
  bool  receive(MessageMem &msg) {
    if (!in_range(msg.phys, MessageMem::MSI_ADDRESS, 1 << 20)) return false;

    COUNTER_INC("MSI");

    unsigned dst = (msg.phys >> 12) & 0xff | (msg.phys << 4) & 0xff00;
    unsigned icr = *msg.ptr & 0xc7ff;
    unsigned event = 1 << ((icr >> 8) & 7);

    // do not forward RRD and SIPI
    if (event & (VCpu::EVENT_RRD | VCpu::EVENT_SIPI)) return false;

    // set logical destination mode
    if (msg.phys & MessageMem::MSI_DM) icr |= MessageApic::ICR_DM;

    // lowest prio mode?
    if (msg.phys & MessageMem::MSI_RH || event & VCpu::EVENT_LOWEST) {
      // we send them round-robin as EVENT_FIXED
      MessageApic msg1(icr & ~0x700, dst, 0);
      return _bus_apic.send_rr(msg1, _lowest_rr);
    }
    MessageApic msg1(icr, dst, 0);
    return _bus_apic.send(msg1);
  }

  Msi(DBus<MessageApic>  &bus_apic) : _bus_apic(bus_apic) {}
};

PARAM_HANDLER(msi,
	      "msi - provide MSI support by forwarding access to 0xfee00000 to the LocalAPICs.")
{
  mb.bus_mem.add(new Msi(mb.bus_apic), Msi::receive_static<MessageMem>);
}

