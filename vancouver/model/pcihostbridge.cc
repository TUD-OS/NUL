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

#include "vmm/motherboard.h"
#include "models/pci.h"


/**
 * A PCI host bridge.
 *
 * State: unstable
 * Features: ConfigSpace
 * Missing: BusReset, LogicalPCI bus
 */
class PciHostBridge : public PciDeviceConfigSpace<PciHostBridge>, public StaticReceiver<PciHostBridge>
{
  DBus<MessagePciCfg> _devices;
  unsigned char  _secondarybus;
  unsigned char  _subord;
  unsigned short _iobase;
  unsigned _confaddress;

  const char *debug_getname() { return "PciHostBridge"; };


  /**
   * Read the PCI config space.
   */
  unsigned read_pcicfg(bool &res)
  {
    MessagePciCfg msg(_confaddress);
    if (_secondarybus == ((_confaddress >> 16) & 0xff))
      {
	msg.address &= 0xfc;
	res = _devices.send(msg, true, (_confaddress >> 8) % PCI_DEVICE_PER_BUS);
      }
    else
      res = _devices.send(msg);
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
	MessagePciCfg msg2(_confaddress + (msg.port & 0x3), value);
	if (_secondarybus == ((_confaddress >> 16) & 0xff))
	  {
	    msg2.address &= 0xff;
	    res = _devices.send(msg2, true, (_confaddress >> 8) % PCI_DEVICE_PER_BUS);
	  }
	else
	  res = _devices.send(msg2);
      }
    else
      return false;
    return true;
  }


  /**
   * Register a device.
   */
  bool receive(MessagePciBridgeAdd &msg)
  {
    _devices.add(msg.dev, msg.func, msg.devfunc);
    return true;
  }
  bool __attribute__((always_inline))  receive(MessagePciCfg &msg)  {    return PciDeviceConfigSpace<PciHostBridge>::receive(msg); };

  PciHostBridge(unsigned busnum, unsigned short iobase) :  _secondarybus(busnum), _subord(busnum), _iobase(iobase), _confaddress(0) {
    MessagePciBridgeAdd msg(0, this, &PciHostBridge::receive_static<MessagePciCfg>);
    receive(msg);
 }
};


REGISTERSET(PciDeviceConfigSpace<PciHostBridge>,
	    REGISTER_RO("ID",  0x0*8, 32, 0x27a08086),
	    REGISTER_RW("CMD", 0x4*8, 16, 0x0106, 0x0106),
	    REGISTER_RO("STS", 0x6*8, 16, 0x0090),
	    REGISTER_RO("CC",  0x9*8, 24, 0x060000),
	    REGISTER_RO("SS", 0x2c*8, 32, 0x27a08086));

PARAM(pcihostbridge,
      {
	unsigned busnum = argv[0];
	PciHostBridge *dev = new PciHostBridge(busnum, argv[1]);
	mb.bus_ioin.add(dev, &PciHostBridge::receive_static<MessageIOIn>);
	mb.bus_ioout.add(dev, &PciHostBridge::receive_static<MessageIOOut>);
	mb.bus_pcibridge.add(dev, &PciHostBridge::receive_static<MessagePciBridgeAdd>, busnum);
      },
      "pcihostbridge:busnr,iobase - attach a pci host bridge to the system.",
      "Example: 'pcihostbridge:0,0xcf8'");
