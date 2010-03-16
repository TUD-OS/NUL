/**
 * Virtual Bios reset routines.
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

/**
 * Virtual Bios reset routines.
 * Features: init of PIC, PIT
 * Missing: serial port, ACPI tables
 */
class VirtualBiosReset : public StaticReceiver<VirtualBiosReset>, public BiosCommon
{
  /**
   * Out to IO-port.
   */
  void outb(unsigned short port, unsigned value)
  {
    MessageIOOut msg(MessageIOOut::TYPE_OUTB, port, value);
    _mb.bus_ioout.send(msg);
  }

  /**
   * called on reset.
   */
  void reset_helper()
  {

    // initialize PIT0
    // let counter0 count with minimal freq of 18.2hz
    outb(0x40+3, 0x24);
    outb(0x40+0, 0);

    // let counter1 generate 15usec refresh cycles
    outb(0x40+3, 0x56);
    outb(0x40+1, 0x12);


    // the master PIC
    // ICW1-4+IMR
    outb(0x20+0, 0x11);
    outb(0x20+1, 0x08); // offset 0x08
    outb(0x20+1, 0x04); // has slave on 2
    outb(0x20+1, 0x0f); // is buffer, master, AEOI and x86
    outb(0x20+1, 0xfc);

    // the slave PIC + IMR
    outb(0xa0+0, 0x11);
    outb(0xa0+1, 0x70); // offset 0x70
    outb(0xa0+1, 0x02); // is slave on 2
    outb(0xa0+1, 0x0b); // is buffer, slave, AEOI and x86
    outb(0xa0+1, 0xff);


    // initilize bios data area
    // we have 640k low memory
    write_bda(0x13, 640, 2);
    // keyboard buffer
    write_bda(0x1a, 0x1e001e, 4);
    write_bda(0x80, 0x2f001e, 4);


#if 0
    // XXX announce number of serial ports
    // announce port
    write_bios_data_area(mb, serial_count * 2 - 2,      argv[0]);
    write_bios_data_area(mb, serial_count * 2 - 2 + 1,  argv[0] >> 8);

    // announce number of serial ports
    write_bios_data_area(mb, 0x11, read_bios_data_area(mb, 0x11) & ~0xf1 | (serial_count*2));
#endif
  };


public:

  bool  receive(MessageBios &msg) {
    switch(msg.irq) {
    case RESET_VECTOR:
      reset_helper();
      return jmp_int(msg, 0x19);
    default:    return false;
    }
  }

  VirtualBiosReset(Motherboard &mb) : BiosCommon(mb) {}
};

PARAM(vbios_reset,
      mb.bus_bios.add(new VirtualBiosReset(mb), &VirtualBiosReset::receive_static<MessageBios>);
      ,
      "vbios_reset - provide reset handling for virtual BIOS functions.");

