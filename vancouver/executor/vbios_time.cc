/**
 * Virtual Bios time routines.
 *
 * Copyright (C) 2009-2010, Bernhard Kauer <bk@vmmon.org>
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
#include "executor/bios.h"

class VirtualBiosTime : public StaticReceiver<VirtualBiosTime>, public BiosCommon
{
  unsigned char tobcd(unsigned char v) { return ((v / 10) << 4) | (v % 10); }

  /**
   * Handle the HW timer IRQ.
   */
  bool handle_int08(CpuState *cpu)
  {
    // Note: no need for EOI since we run in AEOI mode!
    //DEBUG;
    // increment BIOS tick counter
    unsigned ticks = read_bda(0x6c);
    ticks++;
    // midnight?
    if (ticks >= 0x001800B0)
      {
	ticks = 0;
	write_bda(0x70, read_bda(0x70)+1, 1);
      }
    write_bda(0x6c, ticks, 4);

    return jmp_int(cpu, 0x1c);
  }

  /**
   * Time INT handler.
   */
  bool handle_int1a(CpuState *cpu)
  {
    COUNTER_INC("int1a");
    switch (cpu->ah)
      {
      case 0x00: // get system time
	{
	  unsigned ticks = read_bda(0x6c);
	  cpu->al = read_bda(0x70);
	  cpu->dx = ticks;
	  cpu->cx = ticks >> 16;
	  break;
	}
      case 0x01: // set system time
	write_bda(0x6c, static_cast<unsigned>(cpu->cx)<< 16 | cpu->dx, 4);
	write_bda(0x70, 0, 1);
	break;
      case 0x02: // realtime clock
	{
	  unsigned seconds = _mb.clock()->clock(1);
	  cpu->ch = tobcd((seconds / 3600) % 24);
	  cpu->cl = tobcd((seconds / 60) % 60);
	  cpu->dh = tobcd(seconds % 60);
	  cpu->dl = 0;
	  cpu->efl &= ~1;
	  //Logging::printf("realtime clock %x:%x:%x %d\n", cpu->ch, cpu->cl, cpu->dh, seconds);
	  break;
	}
      default:
	VB_UNIMPLEMENTED;
      }
    return true;
  }
public:
  bool  receive(MessageBios &msg) {
    switch(msg.irq) {
    case 0x08:  return handle_int08(msg.cpu);
    case 0x1c:  COUNTER_INC("int1c");
                return true;
    case 0x1a:  return handle_int1a(msg.cpu);
    default:    return false;
    }
  }


  VirtualBiosTime(Motherboard &mb) : BiosCommon(mb) {}
};

PARAM(vbios_time,
      mb.bus_bios.add(new VirtualBiosTime(mb), &VirtualBiosTime::receive_static<MessageBios>);
      ,
      "vbios_time - provide time related virtual BIOS functions.");


