/**
 * CPUID executor.
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
 * Handle CPUID exits.
 *
 * State: testing
 * Features: pentium3
 */
class CpuidExecutor : public StaticReceiver<CpuidExecutor>
{
  const char *debug_getname() { return "CpuidExecutor"; };
 public:
  bool  receive(MessageExecutor &msg)
  {
    // is this our portal?
    assert (msg.cpu->head.pid == 10);

    const char *data;
    switch (msg.cpu->eax)
      {
	// we emulate a pentium3 here with our own strings...
      case 0: data = "\x02\x00\x00\x00NOVAroHV mic"; break;
      case 1: data = "\x73\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xa9\x80\x03"; break;
	// with apic: case 1: data = "\x73\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xa9\x82\x03"; break;
      default:
	Logging::printf("\tCPUID leaf %x at %x\n", msg.cpu->eax, msg.cpu->eip);
	// fall through to the highest basic leaf
      case 2: data = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"; break;
      case 0x80000000: data = "\x04\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"; break;
      case 0x80000001: data = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00\x00"; break;
      case 0x80000002: data = "Vancouver VMM pr"; break;
      case 0x80000003: data = "oudly presents t"; break;
      case 0x80000004: data = "his VirtualCPU. "; break;
      }
    assert(data);
    memcpy(&msg.cpu->eax, data+0x0, 4);
    memcpy(&msg.cpu->ebx, data+0x4, 4);
    memcpy(&msg.cpu->ecx, data+0x8, 4);
    memcpy(&msg.cpu->edx, data+0xc, 4);
    msg.cpu->eip += msg.cpu->inst_len;
    // done
    msg.cpu->head.pid = 0;
    return true;
  };
};

PARAM(cpuid,
      {
	mb.bus_executor.add(new CpuidExecutor(),  &CpuidExecutor::receive_static, 10);
      },
      "cpuid - create a executor that handles the CPUID instruction");

