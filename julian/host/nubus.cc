// -*- Mode: C++ -*-

#include <nul/message.h>
#include <nul/motherboard.h>
#include <cstdint>
#include <service/assert.h>

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


PARAM(nubus,
      {
	HostVfPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
	new NubusManager(pci, mb.clock());
      },
      "nubus - PCI bus manager");

// EOF
