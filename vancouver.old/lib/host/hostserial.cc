/**
 * HostSerial driver.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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

/**
 * A 16550 serial port driver.
 *
 * State: stable
 * Features: receive, FIFO, different LCR+speed
 * Missing: send
 */
class HostSerial : public StaticReceiver<HostSerial>
{
  #include "host/simplehwioin.h"
  #include "host/simplehwioout.h"
  DBus<MessageSerial> & _bus_serial;
  unsigned _serialdev;
  unsigned short _base;
  unsigned _irq;
  unsigned _speed;
  unsigned _lcr;

  const char *debug_getname() { return "HostSerial"; };
  void debug_dump() {  
    Device::debug_dump();
    Logging::printf(" %4x+8 irq %x speed %x _lcr %x", _base, _irq, _speed, _lcr);
  }


  bool get_char(unsigned char &value)
  {
    if (!(inb(_base+0x5) & 1))
      return false;
    value = inb(_base);
    return true;
  }

 public:

  bool  receive(MessageIrq &msg)
  {
    if ((msg.line == _irq) && (msg.type == MessageIrq::ASSERT_IRQ))
      {
	MessageSerial msg2(_serialdev, 0);
	while (get_char(msg2.ch))
	  _bus_serial.send(msg2);
	return true;
      }
    return false;
  }
  
  HostSerial(DBus<MessageIOIn> &bus_hwioin, DBus<MessageIOOut> &bus_hwioout, DBus<MessageSerial> &bus_serial,
	     unsigned serialdev, unsigned base, unsigned irq, unsigned speed, unsigned lcr, unsigned ier)
    : _bus_hwioin(bus_hwioin), _bus_hwioout(bus_hwioout), _bus_serial(bus_serial), 
      _serialdev(serialdev), _base(base), _irq(irq), _speed(speed), _lcr(lcr)
  {

    // enable DLAB and set baudrate
    outb(0x80, _base + 0x3);
    unsigned divisor = speed/115200;
    outb(divisor & 0xff, _base+0x0);
    outb(divisor  >>  8, _base+0x1);
    // disable DLAB and set mode
    outb(lcr & 0x3f, _base+0x3);
    // set IRQ register
    outb(ier & 0x0f, _base+0x1);
    // enable fifo+flush fifos
    outb(0x07, _base+0x2);
    // set RTS,DTR and OUT2 if irqs are enabled
    outb(ier ? 0x0b : 0x03, _base+0x4);

    // consume pending characters
    MessageIrq msg1(MessageIrq::ASSERT_IRQ, _irq);
    receive(msg1);
  };
};


PARAM(hostserial,
      {
	MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, (argv[1] << 8) |  3);
	if (!mb.bus_hostop.send(msg1))
	  Logging::panic("%s failed to allocate ports %lx+8\n", __PRETTY_FUNCTION__, argv[1]);

	Device *dev = new HostSerial(mb.bus_hwioin, mb.bus_hwioout, mb.bus_serial, 
				     argv[0], argv[1], argv[2], argv[3] == ~0UL ? 115200 : argv[3]  , argv[4] == ~0UL ? 3 : argv[4], argv[2] != ~0U);
	mb.bus_hostirq.add(dev, &HostSerial::receive_static<MessageIrq>);

	MessageHostOp msg2(MessageHostOp::OP_ATTACH_HOSTIRQ, argv[2]);
	if (!(msg2.value == ~0U || mb.bus_hostop.send(msg2)))
	  Logging::panic("%s failed to attach hostirq %lx\n", __PRETTY_FUNCTION__, argv[2]);

      },
      "hostserial:hostdevnr,hostiobase,hostirq,speed=115200,lcr=3 - provide an input backend from the host serial port.",
      "Example:   'hostserial:0x4711,0x3f8,4,115200,3'.",
      "The lcr is used to define word-length, length of stop-bit and parity.",
      "See the LCR encoding of the 16550. The default is lcr=3, which means 8N1.");
