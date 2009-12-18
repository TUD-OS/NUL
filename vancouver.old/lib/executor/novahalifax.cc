/**
 * NovaHalifax - second implementation of the instruction emulator.
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
#include "novainstcache.h"

/**
 * NovaHalifax: the second implementation of the instruction emulator.
 */
class NovaHalifax : public StaticReceiver<NovaHalifax> 
{

  const char *debug_getname() { return "NovaHalifax"; };

public:
  bool  receive(MessageExecutor &msg)
  {
    return msg.vcpu->instcache->step(msg);
  }
};

PARAM(novahalifax,
      {
	Logging::printf("create Novahalifax\n");
	mb.bus_executor.add(new NovaHalifax(),  &NovaHalifax::receive_static, 33);
	mb.vcpustate(0)->instcache = new InstructionCache(mb);
      },
      "novahalifax - create a halifax that emulatates instructions.");
