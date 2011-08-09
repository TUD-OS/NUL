/** @file
 * HostSerial driver for PCI MMIO cards.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011,      Michal Sojka <sojka@os.inf.tu-dresden.de>
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
#include "host/hostpci.h"
#include "wvtest.h"

/**
 * A PCI 16550-based serial port driver.
 *
 * State: testing
 * Features: send, receive, FIFO, different LCR+speed
 */
class HostSerialPci : public StaticReceiver<HostSerialPci>
{
  DBus<MessageSerial> & _bus_serial;
  unsigned _serialdev;
  volatile char * _base;
  unsigned _irq;
  unsigned _speed;
  unsigned _lcr;

  unsigned char inb(volatile char *addr)
  {
    return *addr;
  }

  void outb(unsigned char value, volatile char *addr)
  {
    *addr = value;
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


  HostSerialPci(DBus<MessageSerial> &bus_serial,
		unsigned serialdev, char *base, unsigned irq, unsigned clk, unsigned speed, unsigned lcr, unsigned ier)
    : _bus_serial(bus_serial), _serialdev(serialdev), _base(base), _irq(irq), _speed(speed), _lcr(lcr)
  {
    // enable DLAB and set baudrate
    outb(0x80, _base + 0x3);
    unsigned divisor = clk/speed;
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


PARAM_HANDLER(hostserialpci,
	      "hostserialpci:hostdevnr,mask,speed=115200,lcr=3 - provide an in+output backend from the host serial port.",
	      "Example:   'hostserialpci:0,1,115200,3'.",
	      "Bits set to 1 in mask specifies which devices will be handled by this driver"
	      "The lcr is used to define word-length, length of stop-bit and parity.",
	      "See the LCR encoding of the 16550. The default is lcr=3, which means 8N1.",
	      "Received characters are sent to bus_serial with hostdevnr device id and characters send to bus_serial with hostdevnr+1 device id are transmitted.")
{
  HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);

  for (unsigned bdf, num = 0; bdf = pci.search_device(0x7, 0x0, num++);) {
    if (~argv[1] & (1UL << num))
      {
	Logging::printf("Ignore PCI serial controller #%x at %x\n", num, bdf);
	continue;
      }

    unsigned id = pci.conf_read(bdf, 0);
    unsigned offset = 0, clk = 115200;
    switch (id) {
    case 0xc1581415:
      offset = 0x1000; clk = 4000000;
      break;
    default:
      Logging::printf("Unknown serial card bdf:%x id:%08x - ignoring it.\n", bdf, id);
    }

    unsigned irqline = pci.get_gsi(mb.bus_hostop, mb.bus_acpi, bdf, 0);

    Logging::printf("PCI serial controller #%x %x id %x mmio %x\n", num, bdf, pci.conf_read(bdf, 0), pci.conf_read(bdf, HostPci::BAR0));

    MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOMEM, pci.bar_base(bdf, HostPci::BAR0), pci.bar_size(bdf, HostPci::BAR0));
    if (!mb.bus_hostop.send(msg1) || !msg1.ptr)
      Logging::panic("%s failed to allocate ports %lx+8\n", __PRETTY_FUNCTION__, argv[1]);
    char *base = msg1.ptr + offset;

    HostSerialPci *dev = new HostSerialPci(mb.bus_serial, 2*(num-1)+(argv[0] == ~0UL ? 0 : argv[0]),
					   base, irqline, clk,
					   argv[2] == ~0UL ? 115200 : argv[2],
					   argv[3] == ~0UL ? 3 : argv[3], 1);
    mb.bus_hostirq.add(dev, HostSerialPci::receive_static<MessageIrq>);
    mb.bus_serial.add(dev,  HostSerialPci::receive_static<MessageSerial>);
  }
}
