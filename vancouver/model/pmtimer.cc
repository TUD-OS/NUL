/** @file
 * Power Management Timer.
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


/**
 * Power Management Timer.
 *
 * State: unstable
 * Features: Read, FADT
 * Missing: IRQ on overflow
 * Documentation: ACPI spec 3.0b
 */
class PmTimer : public DiscoveryHelper<PmTimer>, public StaticReceiver<PmTimer> {
public:
  Motherboard &_mb;
private:
  unsigned _iobase;
  enum { FREQ = 3579545 };
public:
  bool  receive(MessageIOIn &msg) {

    if (msg.port != _iobase || msg.type != MessageIOIn::TYPE_INL)  return false;
    msg.value = _mb.clock()->clock(FREQ);
    return true;
  }


  void discovery() {

    // write our iobase to the FADT
    discovery_write_dw("FACP",  76,    _iobase, 4);
    discovery_write_dw("FACP",  91,          4, 1);
    discovery_write_dw("FACP", 208, 0x04000401, 4);
    discovery_write_dw("FACP", 212,    _iobase, 4);
    discovery_write_dw("FACP", 216,          0, 4);
  }

  PmTimer(Motherboard &mb, unsigned iobase) : _mb(mb), _iobase(iobase) {

    _mb.bus_ioin.add(this,      receive_static<MessageIOIn>);
    _mb.bus_discovery.add(this, discover);
  }
};

PARAM_HANDLER(pmtimer,
	      "pmtimer:ioport - provide an PMTimer at the given ioport.",
	      "Example: 'pmtimer:0x8000'.")
{
  new PmTimer(mb, argv[0]);
}
