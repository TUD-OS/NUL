/**
 * CPU INIT executor.
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
 * Initialize CPU due to an #INIT.
 *
 * State: testing
 * Features: cpu registers
 */
class InitExecutor : public StaticReceiver<InitExecutor>
{
  const char *debug_getname() { return "InitExecutor"; };
 public:
  bool  receive(MessageExecutor &msg)
  {
    assert(msg.cpu->head._pid == 3);
    Logging::printf(">\t%s mtr %x rip %x il %x cr0 %x efl %x\n", __PRETTY_FUNCTION__, msg.cpu->head.mtr.value(), msg.cpu->eip, msg.cpu->inst_len, msg.cpu->cr0, msg.cpu->efl);

    memset(msg.cpu->msg, 0, sizeof(msg.cpu->msg));
    msg.cpu->eip      = 0xfff0;
    msg.cpu->cr0      = 0x10;
    msg.cpu->cs.ar    = 0x9b;
    msg.cpu->cs.limit = 0xffff;
    msg.cpu->cs.base  = 0xffff0000;
    msg.cpu->ss.ar    = 0x93;
    msg.cpu->edx      = 0x600;
    msg.cpu->ds.ar = msg.cpu->es.ar = msg.cpu->fs.ar = msg.cpu->gs.ar = msg.cpu->ss.ar;
    msg.cpu->ld.ar    = 0x1000;
    msg.cpu->tr.ar    = 0x8b;
    msg.cpu->ss.limit = msg.cpu->ds.limit = msg.cpu->es.limit = msg.cpu->fs.limit = msg.cpu->gs.limit = msg.cpu->cs.limit;
    msg.cpu->tr.limit = msg.cpu->ld.limit = msg.cpu->gd.limit = msg.cpu->id.limit = 0xffff;
    msg.cpu->head.mtr = Mtd(MTD_ALL, 0);
    /*msg.cpu->dr6      = 0xffff0ff0;*/
    msg.cpu->dr7      = 0x400;
    // goto singlestep instruction?
    msg.cpu->efl      = 0;
    msg.cpu->head._pid = 0;
    // init hazards
    msg.vcpu->hazard = VirtualCpuState::HAZARD_CRWRITE | VirtualCpuState::HAZARD_CTRL | (msg.vcpu->hazard & VirtualCpuState::HAZARD_IRQ);
    msg.vcpu->lastmsi = 0;
    return true;
  }
};

PARAM(init,
      {
	mb.bus_executor.add(new InitExecutor(),  &InitExecutor::receive_static, 3);
      },
      "init - create an executor that handles an #INIT signal.");

