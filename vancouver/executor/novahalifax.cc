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

#include "nul/motherboard.h"
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
    if (msg.cpu->head._pid == MessageExecutor::DO_SINGLESTEP)  return msg.vcpu->instcache->step(msg);
    if (msg.cpu->head._pid == MessageExecutor::DO_ENTER)       return msg.vcpu->instcache->enter(msg);
    if (msg.cpu->head._pid == MessageExecutor::DO_LEAVE)       return msg.vcpu->instcache->leave(msg);
    assert(0);
  }
};

PARAM(novahalifax,
      {
	Logging::printf("create Novahalifax\n");
	Device *dev = new NovaHalifax();
	mb.bus_executor.add(dev,  &NovaHalifax::receive_static, MessageExecutor::DO_SINGLESTEP);
	mb.bus_executor.add(dev,  &NovaHalifax::receive_static, MessageExecutor::DO_ENTER);
	mb.bus_executor.add(dev,  &NovaHalifax::receive_static, MessageExecutor::DO_LEAVE);
	for (unsigned i=0; i < Config::NUM_VCPUS; i++)
	  mb.vcpustate(i)->instcache = new InstructionCache(mb);
      },
      "novahalifax - create a halifax that emulatates instructions.");
