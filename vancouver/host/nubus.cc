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

NubusManager::NubusManager(DBus<MessagePciConfig> &pcicfg, Clock *clock)
  : _pcicfg(pcicfg), _clock(clock), _root_bus(*this)
{
  Logging::printf("Nubus initialized.\n");
}


PARAM(nubus,
      {
	new NubusManager(mb.bus_hwpcicfg, mb.clock());
      },
      "nubus - PCI bus manager");

// EOF
