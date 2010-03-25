/**
 * Power Management Timer.
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


/**
 * Power Management Timer.
 *
 * State: unstable
 * Features: Read, FADT
 * Missing: IRQ on overflow
 * Documentation: ACPI spec 3.0b
 */
class PmTimer : public StaticReceiver<PmTimer> {
#include "model/simplediscovery.h"
  Clock *  _clock;
  unsigned _iobase;
  enum {
    FREQ = 3579545,
  };
public:
  bool  receive(MessageIOIn &msg) {

    if (msg.port != _iobase || msg.type != MessageIOIn::TYPE_INL)  return false;
    msg.value = _clock->clock(FREQ);
    return true;
  }


  bool  receive(MessageDiscovery &msg) {
    if (msg.type != MessageDiscovery::DISCOVERY) return false;

    // write our iobase to the FADT
    discovery_write_dw("FACP",  76,    _iobase, 4);
    discovery_write_dw("FACP",  91,          4, 1);
    discovery_write_dw("FACP", 208, 0x04000401, 4);
    discovery_write_dw("FACP", 212,    _iobase, 4);
    discovery_write_dw("FACP", 216,          0, 4);
    // the ACPI IRQ is 9
    discovery_write_dw("FACP",  46,          9, 2);

    return true;
  }

  PmTimer(DBus<MessageDiscovery> &bus_discovery, Clock *clock, unsigned iobase) : _bus_discovery(bus_discovery), _clock(clock), _iobase(iobase) {}
};

PARAM(pmtimer,
      Device *dev = new PmTimer(mb.bus_discovery, mb.clock(), argv[0]);
      mb.bus_ioin.add(dev, PmTimer::receive_static<MessageIOIn>);
      mb.bus_discovery.add(dev, PmTimer::receive_static<MessageDiscovery>);
      ,
      "pmtimer:ioport - provide an PMTimer at the given ioport.",
      "Example: 'pmtimer:0x8000'."
      )
