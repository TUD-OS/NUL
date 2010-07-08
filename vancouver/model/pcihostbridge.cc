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
#include "executor/bios.h"
#include <nul/types.h>

/**
 * A PCI host bridge.
 *
 * State: unstable
 * Features: ConfigSpace, BusReset, MMConfig
 * Missing: LogicalPCI bus
 */

#ifndef REGBASE

class PciHostBridge : public PciConfigHelper<PciHostBridge>, public DiscoveryHelper<PciHostBridge>, public StaticReceiver<PciHostBridge>
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
   * Send a message downstream.
   */
  bool send_bus(MessagePciConfig &msg)
  {
    unsigned bus = msg.bdf >> 8;
    if (!in_range(bus, _busnum, _buscount))  return false;

    // is it our bus?
    if (_busnum == bus) {
      unsigned bdf = msg.bdf; msg.bdf = 0;
      return _mb.bus_pcicfg.send(msg, true, bdf);
    }

    // forward to subordinate busses
    return _mb.bus_pcicfg.send(msg);
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


  /**
   * MMConfig access.
   */
  bool  receive(MessageMem &msg) {
    // XXX Broken for byte and word reads?
    if (!in_range(msg.phys, _membase, _buscount << 20)) return false;

    unsigned bdf = (msg.phys - _membase) >> 12;
    unsigned dword = (msg.phys & 0xfff) >> 2;

    // write
    if (!msg.read) {
      MessagePciConfig msg1(bdf, dword, *msg.ptr);
      return send_bus(msg1);
    }

    // read
    MessagePciConfig msg2(bdf, dword);
    if (!send_bus(msg2)) return false;
    *msg.ptr = msg2.value;
    return true;
  }


  bool receive(MessagePciConfig &msg) { return PciConfigHelper<PciHostBridge>::receive(msg); }


  bool receive(MessageLegacy &msg) {
    if (msg.type != MessageLegacy::RESET) return false;

    PCI_reset();
    _confaddress = 0;
    _cf9 = 0;
    return true;
  }

  // PCI BIOS
  enum {
    // AH
    PCI_FUNCTION_ID     = 0xB1,
    // AL
    PCI_BIOS_PRESENT    = 0x01,
    FIND_PCI_DEVICE     = 0x02,
    FIND_PCI_CLASS_CODE = 0x03,
    READ_CONFIG_BYTE    = 0x08,
    READ_CONFIG_WORD    = 0x09,
    READ_CONFIG_DWORD   = 0x0A,
    WRITE_CONFIG_BYTE   = 0x0B,
    WRITE_CONFIG_WORD   = 0x0C,
    WRITE_CONFIG_DWORD  = 0x0D,
    // Return codes
    SUCCESSFUL          = 0x00,
    FUNC_NOT_SUPPORTED  = 0x81,
    BAD_REGISTER_NUMBER = 0x87,
  };

  bool receive(MessageBios &msg) {
    if ((msg.irq != 0x1a) || (msg.cpu->ah != PCI_FUNCTION_ID))
      return false;

    msg.mtr_out |= MTD_GPR_ACDB;

    // Assume success
    msg.cpu->efl &= ~1;

    switch (msg.cpu->al) {
    case PCI_BIOS_PRESENT:
      msg.cpu->edx  = 0x20494350; // 'PCI '
      msg.cpu->ah   = SUCCESSFUL;
      msg.cpu->al   = 3;	// Config Mechanism 1 and 2, no
				// special cycle generation.
      msg.cpu->bx   = 0x0210;	// PCI BIOS 2.10
      // XXX Last bus in system. Breaks for multiple host bridges.
      msg.cpu->cl   = _busnum;
      return true;
    case READ_CONFIG_BYTE:
    case READ_CONFIG_WORD:
    case READ_CONFIG_DWORD: {
      unsigned order      = msg.cpu->al - READ_CONFIG_BYTE;
      uint16   offs       = msg.cpu->di;
      unsigned byteselect = offs & 3;
      uint32 old = msg.cpu->ecx;

      if ((offs & ((1 << order)-1)) != 0) {
	Logging::printf("Misaligned PCI read reg %x order %u\n", offs, order);
	goto misaligned;
      }

      MessagePciConfig mr(msg.cpu->bx, offs>>2);
      if (!send_bus(mr))
	msg.cpu->efl |= 1;

      static const uint32 pmask[] = { 0xFFFFFF00U, 0xFFFF0000U, 0x00000000U };
      uint32 value = mr.value >> (8*byteselect);
      msg.cpu->ecx = (msg.cpu->ecx & pmask[order]) | (value & ~pmask[order]);
      msg.cpu->ah  = SUCCESSFUL;
      return true;
    }
      misaligned:
      msg.cpu->al   = BAD_REGISTER_NUMBER;
      goto error;
    default:
      DEBUG(msg.cpu);
      msg.cpu->ah   = FUNC_NOT_SUPPORTED;
      // Fallthrough
    }
  error:
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

PARAM(pcihostbridge,
      {
	unsigned busnum = argv[0];
	PciHostBridge *dev = new PciHostBridge(mb, busnum, argv[1], argv[2], argv[3]);

	// ioport interface
	if (~argv[2]) {
	  mb.bus_ioin.add(dev, &PciHostBridge::receive_static<MessageIOIn>);
	  mb.bus_ioout.add(dev, &PciHostBridge::receive_static<MessageIOOut>);
	}

	// MMCFG interface
	if (~argv[3]) {
	  mb.bus_mem.add(dev,       &PciHostBridge::receive_static<MessageMem>);
	  mb.bus_discovery.add(dev, &DiscoveryHelper<PciHostBridge>::receive);
	}

	mb.bus_pcicfg.add(dev, &PciHostBridge::receive_static<MessagePciConfig>, busnum << 8);
	mb.bus_legacy.add(dev, &PciHostBridge::receive_static<MessageLegacy>);
	mb.bus_bios.add  (dev, &PciHostBridge::receive_static<MessageBios>);
      },
      "pcihostbridge:start,count,iobase,membase - attach a pci host bridge to the system.",
      "Example: 'pcihostbridge:0,0x10,0xcf8,0xe0000000'",
      "If not iobase is given, no io-accesses are performed.",
      "Similar if membase is not given, MMCFG is disabled.")
#else
REGSET(PCI,
       REG_RO(PCI_ID,  0x0, 0x27a08086)
       REG_RW(PCI_CMD, 0x1, 0x000900106, 0x0106,)
       REG_RO(PCI_CC,  0x2, 0x06000000)
       REG_RO(PCI_SS,  0xb, 0x27a08086))
#endif
