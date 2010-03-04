/**
 * PCI hostbridge emulation.
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

#include "nul/motherboard.h"
#include "model/pci.h"


/**
 * A PCI host bridge.
 *
 * State: unstable
 * Features: ConfigSpace
 * Missing: BusReset, LogicalPCI bus, MMConfig
 */
class PciHostBridge : public PciDeviceConfigSpace<PciHostBridge>, public StaticReceiver<PciHostBridge>
{
  DBus<MessagePciConfig> &_bus_pcicfg;
  unsigned _secondary;
  unsigned _subordinate;
  unsigned short _iobase;
  unsigned _confaddress;

  const char *debug_getname() { return "PciHostBridge"; };

  /**
   * Send a message downstream.
   */
  bool send_bus(MessagePciConfig &msg)
  {
    bool res = false;
    unsigned bus = msg.bdf >> 8;
    if (_secondary == bus)
      {
	unsigned bdf = msg.bdf;
	msg.bdf = 0;
	res = _bus_pcicfg.send(msg, true, bdf);
      }
    else if (bus >= _secondary && bus <= _subordinate)
	res = _bus_pcicfg.send(msg);
    return res;
  }


  /**
   * Read the PCI config space.
   */
  unsigned read_pcicfg(bool &res)
  {
    MessagePciConfig msg((_confaddress & ~0x80000000) >> 8, (_confaddress & 0xff) >> 2);
    res &= send_bus(msg);
    return msg.value;
  }

public:

  bool receive(MessageIOIn &msg)
  {
    bool res = true;
    if (msg.port == _iobase && msg.type == MessageIOIn::TYPE_INL)
      msg.value = _confaddress;
    else if (in_range(msg.port, _iobase+4, 4) && _confaddress & 0x80000000)
      msg.value = read_pcicfg(res) >> 8*(msg.port & 0x3);
    else
      return false;
    return true;
  }


  bool receive(MessageIOOut &msg)
  {
    /**
     * According to
     * http://www.cs.helsinki.fi/linux/linux-kernel/2003-01/1126.html
     * this is a way to switch between PCI configuration method 2 and
     * 1.  We simply ignore the access.
     */
    if (msg.port == _iobase + 3 && msg.type == MessageIOOut::TYPE_OUTB)
      return true;
    // handle conf access
    else if (msg.port == _iobase && msg.type == MessageIOOut::TYPE_OUTL)
      // PCI spec: the lower two bits are hardwired and must return 0 when read
      _confaddress =  msg.value & ~0x3;
    else if (in_range(msg.port, _iobase+4, 4) && _confaddress & 0x80000000)
      {
	unsigned value = msg.value;
	unsigned shift = 8*(msg.port & 0x3);
	unsigned mask = msg.type == MessageIOOut::TYPE_OUTL ? ~0u : (((1u << 8*(1<<msg.type))-1) << shift);
	bool res;
	if (~mask)  value = (read_pcicfg(res) & ~mask) | ((msg.value << shift) & mask);
	MessagePciConfig msg2((_confaddress & ~0x80000000) >> 8, (_confaddress & 0xff) >> 2, value);
	res &= send_bus(msg2);
      }
    else
      return false;
    return true;
  }


  bool receive(MessagePciConfig &msg) { return PciDeviceConfigSpace<PciHostBridge>::receive(msg); };
  PciHostBridge(DBus<MessagePciConfig> &bus_pcicfg, unsigned secondary, unsigned subordinate, unsigned short iobase)
    : _bus_pcicfg(bus_pcicfg), _secondary(secondary), _subordinate(subordinate), _iobase(iobase), _confaddress(0) {}
};


REGISTERSET(PciDeviceConfigSpace<PciHostBridge>,
	    REGISTER_RO("ID",  0x0, 4, 0x27a08086),
	    REGISTER_RW("CMD", 0x4, 2, 0x0106, 0x0106),
	    REGISTER_RO("STS", 0x6, 2, 0x0090),
	    REGISTER_RO("CC",  0x9, 3, 0x060000),
	    REGISTER_RO("SS", 0x2c, 4, 0x27a08086));

PARAM(pcihostbridge,
      {
	unsigned busnum = argv[0];
	PciHostBridge *dev = new PciHostBridge(mb.bus_pcicfg, busnum, argv[1], ~argv[2] ? argv[2] : 0xcf8);
	mb.bus_ioin.add(dev, &PciHostBridge::receive_static<MessageIOIn>);
	mb.bus_ioout.add(dev, &PciHostBridge::receive_static<MessageIOOut>);
	mb.bus_pcicfg.add(dev, &PciHostBridge::receive_static<MessagePciConfig>, busnum << 8);
      },
      "pcihostbridge:secondary,subordinate,iobase=0xcf8 - attach a pci host bridge to the system.",
      "Example: 'pcihostbridge:0,0xff'");
