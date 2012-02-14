/** @file
 * Host AHCI driver.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
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
#include "host/hostgenericata.h"
#include "host/hostpci.h"


/**
 * The register set of an AHCI port.
 */
union HostAhciPortRegister {
  struct {
    volatile unsigned clb;
    volatile unsigned clbu;
    volatile unsigned fb;
    volatile unsigned fbu;
    volatile unsigned is;
    volatile unsigned ie;
    volatile unsigned cmd;
    volatile unsigned res0;
    volatile unsigned tfd;
    volatile unsigned sig;
    volatile unsigned ssts;
    volatile unsigned sctl;
    volatile unsigned serr;
    volatile unsigned sact;
    volatile unsigned ci;
    volatile unsigned sntf;
    volatile unsigned fbs;
  };
  volatile unsigned regs[32];
};


/**
 * The register set of an AHCI controller.
 */
struct HostAhciRegister {
  union {
    struct {
      volatile unsigned cap;
      volatile unsigned ghc;
      volatile unsigned is;
      volatile unsigned pi;
      volatile unsigned vs;
      volatile unsigned ccc_ctl;
      volatile unsigned ccc_ports;
      volatile unsigned em_loc;
      volatile unsigned em_ctl;
      volatile unsigned cap2;
      volatile unsigned bohc;
    };
    volatile unsigned generic[0x100 >> 2];
  };
  HostAhciPortRegister ports[32];
};


#define check3(X) { unsigned __res = X; if (__res) return __res; }


/**
 * A single AHCI port with its command list and receive FIS buffer.
 *
 * State: testing
 * Supports: read-sectors, write-sectors, identify-drive
 * Missing: ATAPI detection
 */
class HostAhciPort : public StaticReceiver<HostAhciPort>
{
  HostAhciPortRegister volatile *_regs;
  DBus<MessageHostOp> &_bus_hostop;
  DBus<MessageDiskCommit> &_bus_commit;
  Clock *  _clock;
  unsigned _disknr;
  unsigned _max_slots;
  bool     _dmar;
  unsigned *_cl;
  unsigned *_ct;
  unsigned *_fis;
  unsigned _tag;
  HostGenericAta _params;
  unsigned long _usertags[32];
  unsigned _inprogress;
  static const unsigned CL_DWORDS          = 8;
  static const unsigned MAX_PRD_COUNT      = 64;
  // timeout in milliseconds
  static unsigned const FREQ=1000;
  static unsigned const TIMEOUT = 200;

  inline unsigned wait_timeout(volatile unsigned *reg, unsigned mask, unsigned value) {
    timevalue timeout = _clock->clock(FREQ) + TIMEOUT;

    while (((*reg & mask) != value) && _clock->clock(FREQ) < timeout)
      Cpu::pause();
    return (*reg & mask) != value;
  }


  /**
   * Translate a virtual to a physical address.
   */
  void addr2phys(void *ptr, volatile unsigned *dst) {
    unsigned long value = reinterpret_cast<unsigned long>(ptr);
    MessageHostOp msg(MessageHostOp::OP_VIRT_TO_PHYS, value);
    if (!_dmar)
      {
	if (!_bus_hostop.send(msg) || !msg.phys)
	  Logging::panic("could not resolve phys address %lx\n", value);
	value = msg.phys;
      }

    dst[0] = value;
    dst[1] = 0; // support 64bit mode
  }

  void set_command(unsigned char command, unsigned long long sector, bool read, unsigned count = 0,
		   bool atapi = false, unsigned pmp = 0, unsigned features=0)
  {
    _cl[_tag*CL_DWORDS+0] = (atapi ? 0x20 : 0) | (read ? 0 : 0x40) | 5 | ((pmp & 0xf) << 12);
    _cl[_tag*CL_DWORDS+1] = 0;

    // link command list and tables
    addr2phys(_ct + _tag*(128+MAX_PRD_COUNT*16)/4, _cl + _tag*CL_DWORDS + 2);

    // XXX Does any one know how to avoid these type casts in C++0x mode?
#define UC(x) static_cast<unsigned char>(x)
    unsigned char cfis[20] = {0x27, UC(0x80 | (pmp & 0xf)),
                              command, UC(features), UC(sector),
                              UC(sector >> 8), UC(sector >> 16),
                              0x40, UC(sector >> 24), UC(sector >> 32),
                              UC(sector >> 40), UC(features >> 8),
                              UC(count), UC(count >> 8), 0, 0,
                              0, 0, 0, 0};

    memcpy(_ct + _tag*(128 + MAX_PRD_COUNT*16)/4, cfis, sizeof(cfis));
  }


  bool add_dma(char *ptr, unsigned count)
  {
    if (count & 1 || count >> 22) return true;

    unsigned prd = _cl[_tag*CL_DWORDS] >> 16;
    if (prd >= MAX_PRD_COUNT) return true;
    _cl[_tag*CL_DWORDS] += 1<<16;
    unsigned *p = _ct + ((_tag*(128 + MAX_PRD_COUNT*16) + 0x80 + prd*16) >> 2);
    addr2phys(ptr, p);
    p[3] = count - 1;
    return false;
  }


  bool add_prd(void *buffer, unsigned count)
  {
    unsigned prd = _cl[_tag*CL_DWORDS] >> 16;

    assert(~count & 1);
    assert(!(count >> 22));
    if (prd >= MAX_PRD_COUNT) return true;
    _cl[_tag*CL_DWORDS] += 1<<16;
    unsigned *p = _ct + ((_tag*(128 + MAX_PRD_COUNT*16) + 0x80 + prd*16) >> 2);
    addr2phys(buffer, p);
    p[3] = count-1;
    return false;
  }


  unsigned start_command(unsigned long usertag)
  {
    // remember work in progress commands
    _inprogress |= 1 << _tag;
    _usertags[_tag] = usertag;

    _regs->ci =  1 << _tag;
    unsigned res = _tag;
    _tag = (_tag + 1) % _max_slots;
    return res;
  }


  unsigned identify_drive(unsigned short *buffer)
  {
    memset(buffer, 0, 512);
    set_command(0xec, 0, true);
    add_prd(buffer, 512);
    unsigned tag = start_command(0);

    // there is no IRQ on identify, as this is PIO data-in command
    check3(wait_timeout(&_regs->ci, 1 << tag, 0));
    _inprogress &= ~(1 << tag);

    // we do not support spinup
    assert(buffer[2] == 0xc837);
    return _params.update_params(buffer, false);
  }

  unsigned set_features(unsigned features, unsigned count = 0)
  {
    set_command(0xef, 0, false, count, false, 0, features);
    unsigned tag = start_command(0);

    // there is no IRQ on set_features, as this is a PIO command
    check3(wait_timeout(&_regs->ci, 1 << tag, 0));
    _inprogress &= ~(1 << tag);

    return 0;
  }


 public:

  /**
   * Initialize the port.
   */
  unsigned init(unsigned short *buffer)
  {
    if (_regs->cmd & 0xc009) {
      // stop processing by clearing ST
      _regs->cmd &= ~1;
      check3(wait_timeout(&_regs->cmd, 1<<15, 0));

      // stop FIS receiving
      _regs->cmd &= ~0x10;
      // wait until no fis is received anymore
      check3(wait_timeout(&_regs->cmd, 1<<14, 0));
    }

    // set CL and FIS pointer
    addr2phys(_cl,  &_regs->clb);
    addr2phys(_fis, &_regs->fb);

    // clear error register
    _regs->serr = ~0;
    // and irq status register
    _regs->is = ~0;

    // enable FIS processing
    _regs->cmd |= 0x10;
    check3(wait_timeout(&_regs->cmd, 1<<15, 0));

    // CLO clearing
    _regs->cmd |= 0x8;
    check3(wait_timeout(&_regs->cmd, 0x8, 0));
    _regs->cmd |= 0x1;

    // nothing in progress anymore
    _inprogress = 0;

    // enable irqs
    _regs->ie = 0xf98000f1;
    return identify_drive(buffer);
    //set_features(0x3, 0x46);
    //set_features(0x2, 0);
    //return identify_drive(buffer);
  }

  void debug()
  {
    Logging::printf("AHCI is %x ci %x ie %x cmd %x tfd %x\n", _regs->is, _regs->ci, _regs->ie, _regs->cmd, _regs->tfd);
  }

  void irq()
  {
    unsigned is = _regs->is;

    // clear interrupt status
    _regs->is = is;


    for (unsigned done = _inprogress & ~_regs->ci, tag; done; done &= ~(1 << tag))
      {
	tag = Cpu::bsf(done);
	MessageDiskCommit msg2(_disknr, _usertags[tag], MessageDisk::DISK_OK);
	_bus_commit.send(msg2);

	_usertags[tag] = ~0;
	_inprogress &= ~(1 << tag);
      }


    if (_regs->tfd & 1 && ~_regs->tfd & 0x400) {
	Logging::printf("command failed with %x\n", _regs->tfd);
	unsigned short buffer[256];
	init(buffer);
      }

  }


  bool  receive(MessageDisk &msg)
  {
    if (msg.disknr != _disknr)  return false;


    switch (msg.type)
      {
      case MessageDisk::DISK_READ:
      case MessageDisk::DISK_WRITE:
	{
	  unsigned long length = DmaDescriptor::sum_length(msg.dmacount, msg.dma);
	  if (length & 0x1ff)  return false;
	  unsigned char command = _params._lba48 ? 0x25 : 0xc8;
	  if (msg.type == MessageDisk::DISK_WRITE) command = _params._lba48 ? 0x35 : 0xca;
	  set_command(command, msg.sector, msg.type != MessageDisk::DISK_WRITE, length >> 9);


	  for (unsigned i=0; i < msg.dmacount; i++)
	    {
	      if (msg.dma[i].byteoffset > msg.physsize || msg.dma[i].byteoffset + msg.dma[i].bytecount > msg.physsize ||
		  add_dma(reinterpret_cast<char *>(msg.physoffset) + msg.dma[i].byteoffset, msg.dma[i].bytecount)) return false;
	    }
	  start_command(msg.usertag);
	}
	break;
      case MessageDisk::DISK_FLUSH_CACHE:
	set_command(_params._lba48 ? 0xea : 0xe7, 0, true);
	start_command(0);
	break;
      case MessageDisk::DISK_GET_PARAMS:
	_params.get_disk_parameter(msg.params);
	break;
      default:
	Logging::panic("%s %x", __PRETTY_FUNCTION__, msg.type);
      }
    return  true;
  }


  HostAhciPort(HostAhciPortRegister *regs, DBus<MessageHostOp> &bus_hostop, DBus<MessageDiskCommit> &bus_commit, Clock *clock,
	       unsigned disknr, unsigned max_slots, bool dmar)
    : _regs(regs), _bus_hostop(bus_hostop), _bus_commit(bus_commit), _clock(clock), _disknr(disknr), _max_slots(max_slots), _dmar(dmar), _tag(0)
  {
    // allocate needed datastructures
    _fis = new(0x1000) unsigned[1024];
    _cl =  new(0x400)  unsigned[max_slots*CL_DWORDS];
    _ct =  new(0x400)  unsigned[max_slots*(32+MAX_PRD_COUNT*4)];
    Logging::printf("_cl (%p,%p) _ct (%p, %p)\n", _cl, _cl + max_slots*CL_DWORDS, _ct, _ct + max_slots*(128+MAX_PRD_COUNT*16)/4);
  }
};


/**
 * A simple driver for AHCI.
 *
 * State: testing
 * Features: Ports
 */
class HostAhci : public StaticReceiver<HostAhci>
{
  unsigned _bdf;
  unsigned _hostirq;
  HostAhciRegister     *_regs;
  HostAhciPortRegister *_regs_high;
  HostAhciPort *_ports[32];

  void create_ahci_port(unsigned short *buffer, unsigned nr, HostAhciPortRegister *portreg, DBus<MessageHostOp> &bus_hostop, DBus<MessageDisk> &bus_disk, DBus<MessageDiskCommit> &bus_commit, Clock *clock, bool dmar)
  {
    // port implemented and the signature is not 0xffffffff?
    if ((_regs->pi & (1 << nr)) && ~portreg->sig)
      {
	Logging::printf("PORT %x sig %x\n", nr, portreg->sig);
	_ports[nr] = new HostAhciPort(portreg, bus_hostop, bus_commit, clock, bus_disk.count(), ((_regs->cap >> 8) & 0x1f) + 1, dmar);
	if (_ports[nr]->init(buffer))
	  {
	    Logging::printf("AHCI: port %x init failed\n", nr);
	  }
	else
	  bus_disk.add(_ports[nr], HostAhciPort::receive_static<MessageDisk>);
      }
  }

 public:

  HostAhci(HostPci pci, DBus<MessageHostOp> &bus_hostop, DBus<MessageDisk> &bus_disk, DBus<MessageDiskCommit> &bus_commit, Clock *clock, unsigned long bdf, unsigned hostirq, bool dmar)
    : _bdf(bdf), _hostirq(hostirq), _regs_high(0) {

    assert(!(~pci.conf_read(_bdf, 1) & 6) && "we need mem-decode and busmaster dma");
    unsigned long bar = pci.conf_read(_bdf, 9);
    assert(!(bar & 7) && "we need a 32bit memory bar");

    MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, bar, 0x1000);
    if (bus_hostop.send(msg) && msg.ptr)
      _regs = reinterpret_cast<HostAhciRegister *>(msg.ptr);
    else
      Logging::panic("%s could not map the HBA registers", __PRETTY_FUNCTION__);


    // map the high ports
    if (_regs->pi >> 30)
      {
	msg.value = bar + 0x1000;
	if (bus_hostop.send(msg) && msg.ptr)
	  _regs_high = reinterpret_cast<HostAhciPortRegister *>(msg.ptr + (bar & 0xfe0));
	else
	  Logging::panic("%s could not map the high HBA registers", __PRETTY_FUNCTION__);
      }

    // enable AHCI
    _regs->ghc |= 0x80000000;
    Logging::printf("AHCI: cap %x cap2 %x global %x ports %x version %x bohc %x\n", _regs->cap, _regs->cap2, _regs->ghc, _regs->pi, _regs->vs, _regs->bohc);
    assert(!_regs->bohc);


    // create ports
    unsigned short *buffer = new unsigned short[256];
    memset(_ports, 0, sizeof(_ports));
    for (unsigned i=0; i < 30; i++)
      create_ahci_port(buffer, i, _regs->ports+i, bus_hostop, bus_disk, bus_commit, clock, dmar);
    for (unsigned i=30; _regs_high && i < 32; i++)
      create_ahci_port(buffer, i, _regs_high+(i-30), bus_hostop, bus_disk, bus_commit, clock, dmar);
    delete [] buffer;

    // clear pending irqs
    _regs->is = _regs->pi;
    // enable IRQs
    _regs->ghc |= 2;
  }


  bool  receive(MessageIrq &msg) {
    if (msg.line != _hostirq || msg.type == MessageIrq::DEASSERT_IRQ)  return false;
    unsigned is = _regs->is;
    unsigned oldis = is;
    while (is)
      {
	unsigned port = Cpu::bsf(is);
	if (_ports[port]) _ports[port]->irq();
	is &= ~(1 << port);
      }
    _regs->is = oldis;
    return true;
  };

};

PARAM_HANDLER(hostahci,
	      "hostahci:mask - provide a hostdriver for all AHCI controller.",
	      "Example: Use 'hostahci:5' to have a driver for the first and third AHCI controller.",
	      "The mask allows to ignore certain controllers. The default is to use all controllers.")
{
  HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);

  for (unsigned bdf, num = 0; bdf = pci.search_device(0x1, 0x6, num++);) {
    if (~argv[0] & (1UL << num))
      {
	Logging::printf("Ignore AHCI controller #%x at %x\n", num, bdf);
	continue;
      }

    MessageHostOp msg1(MessageHostOp::OP_ASSIGN_PCI, bdf);
    bool dmar = mb.bus_hostop.send(msg1);
    unsigned irqline = pci.get_gsi(mb.bus_hostop, mb.bus_acpi, bdf, 0);

    Logging::printf("DISK controller #%x AHCI %x id %x mmio %x\n", num, bdf, pci.conf_read(bdf, 0), pci.conf_read(bdf, 9));
    HostAhci *dev = new HostAhci(pci, mb.bus_hostop, mb.bus_disk, mb.bus_diskcommit, mb.clock(), bdf, irqline, dmar);
    mb.bus_hostirq.add(dev, HostAhci::receive_static<MessageIrq>);

  }
}
