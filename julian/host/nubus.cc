/** @file
 * PCI bus handling
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include <nul/message.h>
#include <nul/motherboard.h>
#include <host/nubus.h>


// XXX This could be much nicer if device initialization would run
// in a separate thread that can sleep.
void NubusManager::spin(unsigned ms)
{
  timevalue timeout = _clock->abstime(ms, 1000);
  while (_clock->time() < timeout) {
    asm volatile ("pause");
  }
}

NubusManager::NubusManager(HostVfPci pcicfg, Clock *clock)
  : _pci(pcicfg), _clock(clock), _root_bus(*this)
{
  Logging::printf("Nubus initialized.\n");
}


PARAM_HANDLER(nubus,
	      "nubus - PCI bus manager")
{
  HostVfPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
  new NubusManager(pci, mb.clock());
}


// EOF
