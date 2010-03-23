/**
 * MSI support.
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
 * Forward Message Signaled IRQs to the local APICs.
 *
 * State: testing
 * Features: 16bit destination
 */
class Msi  : public StaticReceiver<Msi> {
  enum {
    MSI_ADDRESS = 0xfee00000,
    MSI_SIZE    = 1 << 20,
    MSI_DM      = 1 << 2,
    MSI_RH      = 1 << 3,
  };
  DBus<MessageApic>  & _bus_apic;
  unsigned  _lowest_rr;

public:
  bool  receive(MessageMem &msg) {
    if (!in_range(msg.phys, MSI_ADDRESS, MSI_SIZE)) return false;
    unsigned dst = (msg.phys >> 12) & 0xff | (msg.phys << 4) & 0xff00;
    unsigned icr = *msg.ptr & 0xc7ff;
    unsigned event = 1 << ((icr >> 8) & 7);

    // do not forward RRD and SIPI
    if (event & (VCpu::EVENT_RRD | VCpu::EVENT_SIPI)) return false;

    // set logical destination mode
    if (msg.phys & MSI_DM) icr |= MessageApic::ICR_DM;

    // lowest prio mode?
    if (msg.phys & MSI_RH || event & VCpu::EVENT_LOWEST) {
      // we send them round-robin as EVENT_FIXED
      MessageApic msg1(icr & ~0x700, dst, 0);
      _lowest_rr = _bus_apic.send_rr(msg1, _lowest_rr);
      return _lowest_rr;
    }
    MessageApic msg1(icr, dst, 0);
    return _bus_apic.send(msg1);
  }


  bool  receive(MessageMemRegion &msg)
  {
    if (msg.page != (MSI_ADDRESS >> 12)) return false;
    /**
     * We return true without setting msg.ptr and thus nobody else can
     * claim this region.
     */
    msg.start_page = MSI_ADDRESS >> 12;
    msg.count = MSI_SIZE >> 12;
    return true;
  }

  Msi(DBus<MessageApic>  &bus_apic) : _bus_apic(bus_apic) {}
};

PARAM(msi, {
    Msi *dev = new Msi(mb.bus_apic);
    mb.bus_mem.add(dev,       &Msi::receive_static<MessageMem>);
    mb.bus_memregion.add(dev, &Msi::receive_static<MessageMemRegion>);
  },
  "msi - provide MSI support by forwarding access to 0xfee00000 to the LocalAPICs.");
