/** @file
 * PCI hostbridge emulation.
 *
 * Copyright (C) 2007-2010, Bernhard Kauer <bk@vmmon.org>
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
#include "model/pci.h"
#include "executor/bios.h"

/**
 * A PCI host bridge.
 *
 * State: unstable
 * Features: ConfigSpace, BusReset, MMConfig, PCI BIOS
 * Missing: LogicalPCI bus
 */

#ifndef REGBASE

class PciHostBridge : public DiscoveryHelper<PciHostBridge>, public StaticReceiver<PciHostBridge>
{
public:
  Motherboard &_mb;
private:
  unsigned _busnum;
  unsigned _buscount;
  unsigned short _iobase;
  unsigned long  _membase;
  unsigned _confaddress;
  unsigned char _cf9;
#define  REGBASE "../model/pcihostbridge.cc"
#include "model/reg.h"

  /**
   * Read the PCI config space.
   */
  unsigned read_pcicfg(bool &res)
  {
    MessagePciConfig msg((_confaddress & ~0x80000000) >> 8, (_confaddress & 0xff) >> 2);
    res &= _mb.bus_pcicfg.send(msg);
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
    return res;
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

    /**
     * Reset via 0xcf9 method.
     */
    if (msg.port == _iobase + 1 && msg.type == MessageIOOut::TYPE_OUTB) {
      if (~_cf9 & 4 && msg.value & 4) {
	MessageLegacy msg2(MessageLegacy::RESET);
	_mb.bus_legacy.send(msg2);
      }
      else _cf9 = msg.value;
      return true;
    }


    // handle conf access
    else if (msg.port == _iobase && msg.type == MessageIOOut::TYPE_OUTL)
      // PCI spec: the lower two bits are hardwired and must return 0 when read
      _confaddress =  msg.value & ~0x3;
    else if (in_range(msg.port, _iobase+4, 4) && _confaddress & 0x80000000) {
	unsigned long long value = 0;
	bool res = true;
	if (msg.type != MessageIOOut::TYPE_OUTL) value = read_pcicfg(res);

	// we support unaligned dword accesses here
	Cpu::move(reinterpret_cast<char *>(&value) + (msg.port & 3), &msg.value, msg.type);
	MessagePciConfig msg2((_confaddress & ~0x80000000) >> 8, (_confaddress & 0xff) >> 2, value);
	if (res) res = _mb.bus_pcicfg.send(msg2);
	return res;
      }
    else
      return false;
    return true;
  }


  /**
   * MMConfig access.
   */
  bool  receive(MessageMem &msg) {
    if (!in_range(msg.phys, _membase, _buscount << 20)) return false;

    unsigned bdf = (msg.phys - _membase) >> 12;
    unsigned dword = (msg.phys & 0xfff) >> 2;

    // write
    if (!msg.read) {
      MessagePciConfig msg1(bdf, dword, *msg.ptr);
      return _mb.bus_pcicfg.send(msg1);
    }

    // read
    MessagePciConfig msg2(bdf, dword);
    if (!_mb.bus_pcicfg.send(msg2)) return false;
    *msg.ptr = msg2.value;
    return true;
  }


  bool receive(MessagePciConfig &msg) { return PciHelper::receive(msg, this, _busnum << 8); }
  bool receive(MessageLegacy &msg) {
    if (msg.type != MessageLegacy::RESET) return false;

    PCI_reset();
    _confaddress = 0;
    _cf9 = 0;
    return true;
  }


  /**
   * PCI BIOS functions.
   */
  bool receive(MessageBios &msg) {
    if ((msg.irq != 0x1a) || (msg.cpu->ah != 0xb1))  return false;
    msg.mtr_out |= MTD_GPR_ACDB;

    // Assume success
    msg.cpu->efl &= ~1;
    msg.cpu->ah   = 0;

    switch (msg.cpu->al) {
    case 1: // PCI_BIOS_PRESENT
      msg.cpu->edx  = 0x20494350;// 'PCI '
      msg.cpu->al   = 1;	 // config mechanism 1 only without special cycles
      msg.cpu->bx   = 0x0210;	 // version 2.10
      msg.cpu->cl   = 0xff;      // we support 256 busses
      return true;
    case 0x8 ... 0xa: // READ CONFIG BYTE, WORD, DWORD
      {
	unsigned order      = msg.cpu->al - 0x8;
	unsigned byteselect = msg.cpu->di & 3;

	if (byteselect >> order) {
	  // return BAD_REGISTER_NUMBER on unaligned accesses
	  msg.cpu->al   = 0x87;
	  break;
	}

	MessagePciConfig mr(msg.cpu->bx, msg.cpu->di >> 2);
	if (!_mb.bus_pcicfg.send(mr)) break;

	Cpu::move(&msg.cpu->ecx, reinterpret_cast<char *>(&mr.value) + byteselect, order);
	return true;
      }
    case 0xb ... 0xd: // WRITE CONFIG BYTE, WORD, DWORD
      {
	unsigned order      = msg.cpu->al - 0xb;
	unsigned byteselect = msg.cpu->di & 3;

	if (byteselect >> order) {
	  // return BAD_REGISTER_NUMBER on unaligned accesses
	  msg.cpu->al   = 0x87;
	  break;
	}

	// read the orig word
	MessagePciConfig msg2(msg.cpu->bx, msg.cpu->di >> 2);
	if (!_mb.bus_pcicfg.send(msg2)) break;

	// update the new value
	Cpu::move(reinterpret_cast<char *>(&msg2.value) + byteselect, &msg.cpu->ecx, order);

	msg2.type = MessagePciConfig::TYPE_WRITE;
	if (!_mb.bus_pcicfg.send(msg2)) break;
	return true;
      }
    default:
      DEBUG(msg.cpu);
      msg.cpu->ah   = 0x81; // unsupported function
    }
    // error
    msg.cpu->efl |= 1;
    return true;
  }


  void discovery() {
    unsigned length = discovery_length("MCFG", 44);
    discovery_write_dw("MCFG", length +  0, _membase, 4);
    discovery_write_dw("MCFG", length +  4, static_cast<unsigned long long>(_membase) >> 32, 4);
    discovery_write_dw("MCFG", length +  8, ((_busnum & 0xff) << 16) | (((_buscount-1) & 0xff) << 24) | ((_busnum >> 8) & 0xffff), 4);
    discovery_write_dw("MCFG", length + 12, 0);

    // reset via 0xcf9
    discovery_write_dw("FACP", 116, 0x01000801, 4);
    discovery_write_dw("FACP", 120, 0xcf9, 4);
    discovery_write_dw("FACP", 124, 0, 4);
    discovery_write_dw("FACP", 128, 6, 1);
  }


  PciHostBridge(Motherboard &mb, unsigned busnum, unsigned buscount, unsigned short iobase, unsigned long membase)
    :  _mb(mb), _busnum(busnum), _buscount(buscount), _iobase(iobase), _membase(membase) {}
};

PARAM_HANDLER(pcihostbridge,
	      "pcihostbridge:start,count,iobase,membase - attach a pci host bridge to the system.",
	      "Example: 'pcihostbridge:0,0x10,0xcf8,0xe0000000'",
	      "If not iobase is given, no io-accesses are performed.",
	      "Similar if membase is not given, MMCFG is disabled.")
{
  unsigned busnum = argv[0];
  PciHostBridge *dev = new PciHostBridge(mb, busnum, argv[1], argv[2], argv[3]);

  // ioport interface
  if (~argv[2]) {
    mb.bus_ioin.add(dev,  PciHostBridge::receive_static<MessageIOIn>);
    mb.bus_ioout.add(dev, PciHostBridge::receive_static<MessageIOOut>);
  }

  // MMCFG interface
  if (~argv[3]) {
    mb.bus_mem.add(dev,       PciHostBridge::receive_static<MessageMem>);
    mb.bus_discovery.add(dev, PciHostBridge::discover);
  }

  mb.bus_pcicfg.add(dev, PciHostBridge::receive_static<MessagePciConfig>);
  mb.bus_legacy.add(dev, PciHostBridge::receive_static<MessageLegacy>);
  mb.bus_bios.add  (dev, PciHostBridge::receive_static<MessageBios>);
}
#else
REGSET(PCI,
       REG_RO(PCI_ID,  0x0, 0x27a08086)
       REG_RW(PCI_CMD, 0x1, 0x000900106, 0x0106,)
       REG_RO(PCI_CC,  0x2, 0x06000000)
       REG_RO(PCI_SS,  0xb, 0x27a08086))
#endif
