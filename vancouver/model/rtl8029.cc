/** @file
 * RTL8029 emulation - a ne2k compatible PCI network card.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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

/**
 * RTL8029 device model.
 *
 * State: unstable
 * Features: PCI, send, receive, broadcast, promiscuous mode
 * Missing: multicast, CRC calculation, rep optimized
 */
#ifndef REGBASE
class Rtl8029: public StaticReceiver<Rtl8029>
{
  DBus<MessageNetwork>  &_bus_network;
  DBus<MessageIrqLines> &_bus_irqlines;
  unsigned char _irq;
  unsigned long long _mac;
  unsigned _bdf;
  struct {
    unsigned char  cr;
    unsigned short clda;
    unsigned char  bnry;
    unsigned char  tsr;
    unsigned char  ncr;
    unsigned char  fifo;
    unsigned char  isr;
    unsigned short crda;
    unsigned short id8029;
    unsigned char  rsr;
    unsigned char  cntr[3];
    unsigned char  _cr1;
    unsigned char  par[6];
    unsigned char  curr;
    unsigned char  mar[8];
    unsigned char  _cr2;
    unsigned char  pstart;
    unsigned char  pstop;
    unsigned char  _bnry2;
    unsigned char  tpsr;
    unsigned short tbcr;
    unsigned char  _isr2;
    unsigned short rsar;
    unsigned short rbcr;
    unsigned char rcr;
    unsigned char tcr;
    unsigned char dcr;
    unsigned char imr;
  } __attribute__((packed)) _regs;
  unsigned char _mem[65536];
#define  REGBASE "../model/rtl8029.cc"
#include "model/reg.h"


  void update_isr(unsigned value)
  {
    _regs.isr |= value;
    if (_regs.isr & _regs.imr)
      {
	MessageIrqLines msg(MessageIrq::ASSERT_IRQ, _irq);
	_bus_irqlines.send(msg);
      }
  }


  void send_packet()
  {
    COUNTER_INC("SEND packet");
    // check for buffer overflows or short packets
    if (((_regs.tpsr << 8) + _regs.tbcr) < static_cast<int>(sizeof(_mem)) && _regs.tbcr >= 8u)
      {
	MessageNetwork msg2(_mem + (_regs.tpsr << 8), _regs.tbcr, 0);
	_bus_network.send(msg2);
	_regs.tsr = 0x1;
	update_isr(0x2);
      }
    else
      {
	// transmit error
	_regs.tsr = 0x20;
	update_isr(0x10);
      }
    _regs.cr &= ~4;
  }

  bool not_accept(const unsigned char *buffer, unsigned len)
  {
    // small packet?
    if ((len < 60 && ~_regs.rcr & 2) || len < 8)  return true;

    // broadcast or multicast
    if (buffer[0] & 0x1)
      {
	if (_regs.rcr & 4 && !memcmp(buffer, "\xff\xff\xff\xff\xff\xff", 6)) return false;
	// XXX multicast!!
	_regs.rsr |= 0x20;
	return true;
      }
    else
      return (~_regs.rcr & 0x10) && memcmp(buffer, _regs.par, 6) && (~_regs.rcr & 0x10);
  }

  bool receive_packet(const unsigned char *buffer, unsigned len)
  {
    COUNTER_INC("RECV packet");
    // clear status bits, except receiver disabled
    _regs.rsr &= 0x40;
    if (not_accept(buffer, len)) return false;

    COUNTER_INC("RECV accept");
    COUNTER_SET("RECV cr", _regs.cr);
    COUNTER_SET("RECV bnry", _regs.bnry);
    COUNTER_SET("RECV curr", _regs.curr);

    // stopped card or in monitor mode
    if (_regs.cr & 1 && _regs.rcr & 0x20)
      {
	_regs.rsr |= 0x10;
	if (_regs.cntr[2] < 0xc0 && ++_regs.cntr[2] & 0x80) update_isr(0x20);
	return false;
      }


    unsigned start = _regs.curr << 8;
    len += 4;
    _mem[start + 2] = len ;
    _mem[start + 3] = len >> 8;
    unsigned space = _regs.pstop - _regs.curr;
    bool overflow = false;
    if (_regs.bnry > _regs.curr) space = _regs.bnry - _regs.curr - 1;
    space = (space << 8) - 4;
    memcpy(_mem + start + 4, buffer, space < len ? space : (len - 4));
    if (space < len)
      {
	buffer += space;
	len -= space;
	unsigned space2 = (_regs.bnry - _regs.pstart - 1) << 8;
	memcpy(_mem + start + 4 + space, buffer, space2 < len ? space2 : len - 4);
	overflow = space2 < len + 4;
	_regs.curr = _regs.pstart + ((len + 255 + 4) >> 8);
      }
    else
      {
	overflow = space < len;
	_regs.curr += (len + 4 + 255) >> 8;
	if (_regs.curr == _regs.pstop) _regs.curr = _regs.pstart;
      }
    if (overflow)
      {
	Logging::printf("overflow %x\n", _regs.curr);
	_regs.curr = start >> 8;
	_regs.cr = 1;
	update_isr(0x90);
	return false;
      }
    assert(_regs.curr < _regs.pstop && _regs.curr >= _regs.pstart);
    _regs.rsr |= 1;
    _mem[start + 1] = _regs.curr;
    //if (1 || _regs.rcr & 1)
    _mem[start] = _regs.rsr;
    // XXX CRC
    update_isr(0x1);
    return true;
  }

  void read_byte(unsigned addr, unsigned char *value)
  {
    if (in_range(addr, 0x10, 8)) // remote DMA?
      {
	// check that a read op is in progress!
	if (_regs.rbcr && ((_regs.cr & 0x38)== 0x8))
	  {
	    *value = _mem[_regs.rsar++];
	    if (!--_regs.rbcr)  update_isr(0x40);
	  }
      }
    else
      if (!addr) *value = _regs.cr;
      else
	{
	  unsigned ofs = addr + ((_regs.cr >> 6) << 4);
	  if (ofs < sizeof( _regs))
	    *value = reinterpret_cast<unsigned char *>(&_regs)[ofs];
	  // clear cnt on read
	  if (ofs >= 0xd && ofs < 0x10)
	    _regs.cntr[ofs-0xd] = 0;
	}
  }


  void write_byte(unsigned addr, unsigned value)
  {
    if (addr >= 0x18)  // reset?
      {
	// stop the card
	_regs.cr = 1;
	update_isr(0x80);
      }
    else if (addr >= 0x10) // remote DMA?
      {
	// check that a write op is in progress!
	if (_regs.rbcr && ((_regs.cr & 0x38)== 0x10))
	  {
	    // the first page is read-only
	    if (_regs.rsar >= 0x100) _mem[_regs.rsar] = value;
	    _regs.rsar++;
	    if (!--_regs.rbcr)  update_isr(0x40);
	  }
      }
    else if (!addr)
      {
	// keep the transmit bit
	_regs.cr = value | (_regs.cr & 4);
	if (~_regs.cr & 0x1)
	  {
	    // clear reset indicator
	    _regs.isr &= ~0x80;

	    // send packet
	    if ((_regs.cr & 0x5) == 0x4) send_packet();

	    // check empty remote DMA request
	    if (((_regs.cr ^ 0x20) & 0x38) && !_regs.rbcr) update_isr(40);
	  }
      }
    else
      {
	unsigned reg = addr + (_regs.cr & 0xc0);
	switch (reg)
	  {
	  case 0x3: _regs.bnry = value;   break;
	  case 0x7: _regs.isr &= ~value | 0x80; break;
	  case 0xd: value &= 0x1f;
	  case 0xc: value &= 0x3f;
	  case 0xe:
	  case 0xf: value &= 0x7f;
	  case 0x1:
	  case 0x2:
	  case 0x4 ... 0x6:
	  case 0x8 ... 0xb:
	    (&_regs.pstart)[addr - 1] = value;
	    break;
	  case 0x41 ... 0x4f:
	    _regs.par[addr - 1] = value;
	    break;
	  }
	if (reg == 0xc) _regs.rsr = (_regs.rsr & 0xbf) | ((_regs.rcr & 0x20) << 1);
	if (reg == 0xf) update_isr(0);
      }
  }

  bool match_bar(unsigned long &address) {
    bool res = !((address ^ PCI_BAR) & PCI_BAR_mask);
    address &= ~PCI_BAR_mask;
    return res;
  }

public:
  bool  receive(MessageNetwork &msg)
  {
    if (msg.buffer >= _mem && msg.buffer < _mem + sizeof(_mem)) return false;
    return receive_packet(msg.buffer, msg.len);
  }

  bool receive(MessageIOIn &msg)
  {
    unsigned long addr = msg.port;
    if (!match_bar(addr) || !(PCI_CMD_STS & 0x1))
      return false;

    // for every byte
    for (unsigned i = 0; i < (1u<<msg.type); i++, addr++)
      read_byte(addr, reinterpret_cast<unsigned char *>(&msg.value)+i);

    return true;
  }


  bool receive(MessageIOOut &msg)
  {
    unsigned long addr = msg.port;
    if (!match_bar(addr) || !(PCI_CMD_STS & 0x1))
      return false;

    for (unsigned i = 0; i < (1u<<msg.type); i++, addr++)
      write_byte(addr, msg.value >> (i*8));
    return true;
  }

  bool receive(MessagePciConfig &msg)  {  return PciHelper::receive(msg, this, _bdf); }


  Rtl8029(DBus<MessageNetwork> &bus_network, DBus<MessageIrqLines> &bus_irqlines, unsigned char irq, unsigned long long mac, unsigned bdf) :
    _bus_network(bus_network), _bus_irqlines(bus_irqlines),  _irq(irq), _mac(mac), _bdf(bdf)
  {
    PCI_reset();

    // init memory
    memset(_mem, 0x00, sizeof(_mem));
    for (unsigned i=0; i< 6; i++)  _mem[i] = reinterpret_cast<unsigned char *>(&_mac)[5 - i];
    memcpy(_mem + 0xe, "WW", 2);

    for (unsigned i=0; i< 6; i++)  _mem[i + 0x10] = reinterpret_cast<unsigned char *>(&_mac)[5 - i];
    memcpy(_mem + 0x1e, "BB", 2);

    for (unsigned i=1; i<8; i++) memcpy(_mem + 0x20*i, _mem, 0x20);

    // and the read-only regs
    _regs.id8029 = 0x4350;
  }
};


PARAM_HANDLER(rtl8029,
	      "rtl8029:bdf,irq,ioio - attach an rtl8029 (ne2k compatible) network controller to the PCI bus",
	      "Example: 'rtl8029:,9,0x300'.",
	      "If no bdf is given a free one is used.")
{
  MessageHostOp msg(MessageHostOp::OP_GET_MAC, 0UL);
  if (!mb.bus_hostop.send(msg))  Logging::panic("Could not get a MAC address");
  Rtl8029 *dev = new Rtl8029(mb.bus_network, mb.bus_irqlines, argv[1], msg.mac, PciHelper::find_free_bdf(mb.bus_pcicfg, argv[0]));
  mb.bus_pcicfg.add (dev, Rtl8029::receive_static<MessagePciConfig>);
  mb.bus_ioin.add   (dev, Rtl8029::receive_static<MessageIOIn>);
  mb.bus_ioout.add  (dev, Rtl8029::receive_static<MessageIOOut>);
  mb.bus_network.add(dev, Rtl8029::receive_static<MessageNetwork>);


  // set IO region and IRQ
  dev->PCI_write(Rtl8029::PCI_INTR_offset, argv[1]);
  dev->PCI_write(Rtl8029::PCI_BAR_offset,  argv[2]);

  // set default state, this is normally done by the BIOS
  // enable IO accesses
  dev->PCI_write(Rtl8029::PCI_CMD_STS_offset, 1);
}

#else
REGSET(PCI,
       REG_RO(PCI_ID,       0x0, 0x802910ec)
       REG_RW(PCI_CMD_STS,  0x1, 0x02000000, 0x0003,)
       REG_RO(PCI_RID_CC,   0x2, 0x02000000)
       REG_RW(PCI_BAR,      0x4, 1, 0xffffffe0,)
       REG_RO(PCI_SS,       0xb, 0x802910ec)
       REG_RW(PCI_INTR,     0xf, 0x0100, 0x0f,));
#endif
