/** @file
 * Virtual Bios time routines.
 *
 * Copyright (C) 2009-2010, Bernhard Kauer <bk@vmmon.org>
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

#include "nul/motherboard.h"
#include "executor/bios.h"

class VirtualBiosTime : public StaticReceiver<VirtualBiosTime>, public BiosCommon
{
  unsigned char tobcd(unsigned char v) { return ((v / 10) << 4) | (v % 10); }

  /**
   * Handle the HW timer IRQ.
   */
  bool handle_int08(MessageBios &msg)
  {
    // Note: no need for EOI since we run in AEOI mode!
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

    return jmp_int(msg, 0x1c);
  }

  /**
   * Time INT handler.
   */
  bool handle_int1a(MessageBios &msg)
  {
    COUNTER_INC("int1a");
    switch (msg.cpu->ah)
      {
      case 0x00: // get system time
	{
	  unsigned ticks = read_bda(0x6c);
	  msg.cpu->al = read_bda(0x70);
	  msg.cpu->dx = ticks;
	  msg.cpu->cx = ticks >> 16;
	  msg.mtr_out |= MTD_GPR_ACDB;
	  break;
	}
      case 0x01: // set system time
	write_bda(0x6c, static_cast<unsigned>(msg.cpu->cx)<< 16 | msg.cpu->dx, 4);
	write_bda(0x70, 0, 1);
	break;
      case 0x02: // realtime clock
	{
	  // XXX use the RTC time
	  unsigned seconds = _mb.clock()->clock(1);
	  msg.cpu->ch = tobcd((seconds / 3600) % 24);
	  msg.cpu->cl = tobcd((seconds / 60) % 60);
	  msg.cpu->dh = tobcd(seconds % 60);
	  msg.cpu->dl = 0;
	  msg.cpu->efl &= ~1;
	  msg.mtr_out |= MTD_GPR_ACDB | MTD_RFLAGS;
	  break;
	}
      default:
	// PCI BIOS is implemented in pcihostbridge, so don't consume
	// the message here.
	return false;
      }
    return true;
  }
public:
  bool  receive(MessageBios &msg) {
    switch(msg.irq) {
    case 0x08:  return handle_int08(msg);
    case 0x1c:  COUNTER_INC("int1c");
                return true;
    case 0x1a:  return handle_int1a(msg);
    default:    return false;
    }
  }


  VirtualBiosTime(Motherboard &mb) : BiosCommon(mb) {}
};

PARAM_HANDLER(vbios_time,
	      "vbios_time - provide time related virtual BIOS functions.")
{
  mb.bus_bios.add(new VirtualBiosTime(mb), VirtualBiosTime::receive_static<MessageBios>);
}


