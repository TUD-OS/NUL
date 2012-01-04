/** @file
 * HostSerial driver.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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

/**
 * A 16550 serial port driver.
 *
 * State: stable
 * Features: send, receive, FIFO, different LCR+speed
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


  bool  receive(MessageSerial &msg)
  {
    if (msg.serial != _serialdev + 1)  return false;
    for (unsigned i=0; i < 10000; i++)
      {
	if (inb(_base+5) & 0x20) break;
	Cpu::pause();
      }
    outb(msg.ch, _base);
    return true;
  }


  HostSerial(DBus<MessageHwIOIn> &bus_hwioin, DBus<MessageHwIOOut> &bus_hwioout, DBus<MessageSerial> &bus_serial,
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


PARAM_HANDLER(hostserial,
	      "hostserial:hostdevnr,hostiobase,hostirq,speed=115200,lcr=3 - provide an in+output backend from the host serial port.",
	      "Example:   'hostserial:0x4711,0x3f8,4,115200,3'.",
	      "If no iobase is given, the first serial port from the BIOS is used.",
	      "The lcr is used to define word-length, length of stop-bit and parity.",
	      "See the LCR encoding of the 16550. The default is lcr=3, which means 8N1.",
	      "The output is received from hostdevnr+1.")
{
  unsigned iobase = argv[1];
  
  if (iobase == ~0u) {
    MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, 0x400, 0x1000);
    if (mb.bus_hostop.send(msg) && msg.ptr)
      iobase = *reinterpret_cast<unsigned short *>(msg.ptr);

    if (iobase == 0) {
      Logging::printf("Couldn't find ports for serial controller.\n");
      return;
    } else
      Logging::printf("Serial ports found at 0x%x\n", iobase);
  }

  MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION,  (iobase << 8) |  3);
  if (!mb.bus_hostop.send(msg1))
    Logging::panic("%s failed to allocate ports %lx+8\n", __PRETTY_FUNCTION__, argv[1]);

  HostSerial *dev = new HostSerial(mb.bus_hwioin, mb.bus_hwioout, mb.bus_serial,
			       argv[0] == ~0UL ? 0 : argv[0], iobase, argv[2],
			       argv[3] == ~0UL ? 115200 : argv[3],
			       argv[4] == ~0UL ? 3 : argv[4], argv[2] != ~0U);
  mb.bus_hostirq.add(dev, HostSerial::receive_static<MessageIrq>);
  mb.bus_serial.add(dev,  HostSerial::receive_static<MessageSerial>);

  MessageHostOp msg2 = MessageHostOp::attach_irq(argv[2], ~0U, true, "serial");
  if (!(argv[2] == ~0UL || mb.bus_hostop.send(msg2)))
    Logging::panic("%s failed to attach hostirq %lx\n", __PRETTY_FUNCTION__, argv[2]);

}
