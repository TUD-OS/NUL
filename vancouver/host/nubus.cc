// -*- Mode: C++ -*-

#include <vmm/message.h>
#include <vmm/motherboard.h>
#include <cstdint>
#include <cassert>

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

NubusManager::NubusManager(HostPci pcicfg, Clock *clock)
  : _pci(pcicfg), _clock(clock), _root_bus(*this)
{
  Logging::printf("Nubus initialized.\n");
}


PARAM(nubus,
      {
	HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
	new NubusManager(pci, mb.clock());
      },
      "nubus - PCI bus manager");

// EOF
