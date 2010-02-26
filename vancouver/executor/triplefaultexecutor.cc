/**
 * TripleFault executor.
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

/**
 * Handle TripleFault exits.
 *
 * State: stable
 */
class TripleFaultExecutor : public StaticReceiver<TripleFaultExecutor>
{
  DBus<MessageLegacy> &_bus_legacy;
  const char *debug_getname() { return "TripleFaultExecutor"; };
 public:
  bool  receive(MessageExecutor &msg)
  {
    Logging::printf(">\t%s mtr %x rip %x il %x cr0 %x efl %x pid %x\n", __PRETTY_FUNCTION__, msg.cpu->head.mtr.value(), msg.cpu->eip, msg.cpu->inst_len, msg.cpu->cr0, msg.cpu->efl, msg.cpu->head.pid);
    assert(msg.cpu->head.pid == 2);

    MessageLegacy msg1(MessageLegacy::RESET, 0);
    _bus_legacy.send_fifo(msg1);
    return true;
  }

  TripleFaultExecutor(DBus<MessageLegacy> &bus_legacy) : _bus_legacy(bus_legacy) {}
};

PARAM(triplefault,
      {
	mb.bus_executor.add(new TripleFaultExecutor(mb.bus_legacy),  &TripleFaultExecutor::receive_static, 2);
      },
      "triplefault - create a executor that handles a triplefault by doing a reset.");

