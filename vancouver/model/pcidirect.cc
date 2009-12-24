/**
 * Directly-assigned PCI device.
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
#include "host/hostpci.h"


/**
 * Directly assign a host PCI device to the guest.
 *
 * State: testing
 * Features: pcicfgspace, ioport operations, memory read/write, host irq
 * Missing: DMA remapping, MSI
 * Documentation: PCI spec v.2.2
 */
class DirectPciDevice : public StaticReceiver<DirectPciDevice>, public HostPci
{
  Motherboard &_mb;
  enum
    {
      BARS = 6,
      PCI_CFG_SPACE_MASK = 0xff,
      PCI_CFG_SPACE_DWORDS = 64,
    };
  unsigned _address;
  unsigned _hostirq;
  unsigned _cfgspace[PCI_CFG_SPACE_DWORDS];
  unsigned _bars[BARS];
  unsigned _masks[BARS];
  const char *debug_getname() { return "DirectPciDevice"; }


  /**
   * Induce the number of the bars from the header-type.
   */
  unsigned count_bars()
  {
    switch((_cfgspace[3] >> 24) & 0x7f)
      {
      case 0: return 6;
      case 1: return 2;
      default: return 0;
      }
  }


  /**
   * Read the bars and the corresponding masks.
   */
  void __attribute__((always_inline)) read_bars()
    {

      // disable device
      unsigned cmd = conf_read(_address + 0x4);
      conf_write(_address + 0x4, cmd & ~0x7);

      // read bars and masks
      for (unsigned i=0; i < count_bars(); i++)
	{
	  unsigned a = _address + 0x10 + i*4;
	  _bars[i] = conf_read(a);
	  conf_write(a, ~0U);
	  _masks[i] = conf_read(a);
	  conf_write(a, _bars[i]);
	}
      // reenable device
      conf_write(_address + 0x4, cmd);


      for (unsigned i=0; i < count_bars(); i++)
	{

	  Logging::printf("%s() bar %x mask %x\n", __func__, _bars[i], _masks[i]);
	  unsigned  bar = _bars[i];
	  if (bar)
	    if ((bar & 1) == 1)
	      {
		MessageHostOp msg(MessageHostOp::OP_ALLOC_IOIO_REGION, ((bar & ~3) << 8) |  Cpu::bsf((~_masks[i] | 0x3)));
		_mb.bus_hostop.send(msg);
	      }
	    else
	      {
		MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, bar & ~0x1f, 1 << Cpu::bsr((~_masks[i] | 0xf) + 1));
		if (_mb.bus_hostop.send(msg) && msg.ptr)
		  _bars[i] = reinterpret_cast<unsigned long>(msg.ptr) + (bar & 0x10);
		else
		  Logging::panic("can not map IOMEM region %lx+%x", msg.value, msg.len);
	      }
	  // skip upper part of 64bit bar
	  if ((bar & 0x6) == 0x4)
	    {
	      i++;
	      _masks[i] = 0;
	    }
	}
    }

  /**
   * Check whether the guest io address matches and translate to host
   * address.
   */
  bool match_bars(unsigned address, unsigned size, bool iospace)
  {
    // check whether io decode is disabled
    if (iospace && ~_cfgspace[1] & 1 || !iospace && ~_cfgspace[1] & 2)
      return false;
    for (unsigned i=0; i < count_bars(); i++)
      {
	unsigned  bar = _cfgspace[4 + i];
	// XXX prefetch bit
	if (!_masks[i] || (bar & 1) != iospace || !in_range(address, bar & ~0x3, (~_masks[i] | 3) + 1 - size + 1))
	  continue;
	address = address - bar + _bars[i];
	return true;
      }
    return false;
  }

 public:


  bool  receive(MessageIOIn &msg)
  {
    unsigned old_port = msg.port;
    unsigned new_port = msg.port;
    if (!match_bars(new_port, 1 << msg.type, true))  return false;
    msg.port = new_port;
    bool res = _mb.bus_hwioin.send(msg);
    msg.port = old_port;
    return res;
  }


  bool  receive(MessageIOOut &msg)
  {
    unsigned old_port = msg.port;
    unsigned new_port = msg.port;
    if (!match_bars(new_port, 1 << msg.type, true))  return false;
    msg.port = new_port;
    bool res = _mb.bus_hwioout.send(msg);
    msg.port = old_port;
    return res;
  }


  bool __attribute__((always_inline))  receive(MessagePciCfg &msg)
  {
    if (!(msg.address & ~0xff))
      {
	bool res;
	if (msg.type == MessagePciCfg::TYPE_READ)
	  {
	    assert(msg.address <= PCI_CFG_SPACE_MASK);
	    if (in_range(msg.address, 0x10, BARS * 4))
	      memcpy(&msg.value, reinterpret_cast<char *>(_cfgspace) + msg.address, 4);
	    else
	      msg.value = conf_read(_address + msg.address & ~0x3) >> (8 * (msg.address & 3));

	    // disable capabilities, thus MSI is not known
	    if (msg.address == 0x4)   msg.value &= ~0x10;
	    if (msg.address == 0x34)  msg.value &= ~0xff;

	    // disable multi-function devices
	    if (msg.address == 0xc)   msg.value &= ~0x800000;
	    Logging::printf("%s:%x -- %x,%x\n", __PRETTY_FUNCTION__, _address, msg.address, msg.value);
	    return true;
	  }
	else
	  {
	    assert(msg.address <= PCI_CFG_SPACE_MASK);
	    assert(!(msg.address & 3));
	    unsigned mask = ~0u;
	    if (in_range(msg.address, 0x10, BARS * 4))  mask &= _masks[(msg.address - 0x10) >> 2];
	    _cfgspace[msg.address >> 2] = _cfgspace[msg.address >> 2] & ~mask | msg.value & mask;

	    // write through if not in the bar-range
	    if (!in_range(msg.address, 0x10, BARS * 4) && msg.address)
	      conf_write(_address + msg.address, _cfgspace[msg.address >> 2]);

	    Logging::printf("%s:%x -- %x,%x dev %x\n", __PRETTY_FUNCTION__, _address, msg.address, _cfgspace[msg.address >> 2], conf_read(_address + msg.address));
	  return true;
	  }
      }
    return false;
  }


  bool  receive(MessageIrq &msg)
  {
    if (msg.line != _hostirq)  return false;
    Logging::printf("Forwarding irq message #%x  %x -> %x\n", msg.type, msg.line, _cfgspace[15] & 0xff);
    MessageIrq msg2(msg.type, _cfgspace[15] & 0xff);
    if (msg.type == MessageIrq::ASSERT_IRQ)  msg2.type = MessageIrq::ASSERT_NOTIFY;
    return _mb.bus_irqlines.send(msg2);
  }


  bool  receive(MessageIrqNotify &msg)
  {
    unsigned irq = _cfgspace[15] & 0xff;
    if (in_range(irq, msg.baseirq, 8) && msg.mask & (1 << (irq & 0x7)))
      {
	Logging::printf("Notify irq message #%x  %x -> %x\n", msg.mask, msg.baseirq, _hostirq);
	MessageHostOp msg2(MessageHostOp::OP_UNMASK_IRQ, _hostirq);
	return _mb.bus_hostop.send(msg2);
      }
    return false;
  }


  bool  receive(MessageMemWrite &msg)
  {
    unsigned addr = msg.phys;
    if (!match_bars(addr, msg.count, false))  return false;
    if (msg.count == 4)  *reinterpret_cast<unsigned *      >(addr) = *reinterpret_cast<unsigned *      >(msg.ptr);
    if (msg.count == 2)  *reinterpret_cast<unsigned short *>(addr) = *reinterpret_cast<unsigned short *>(msg.ptr);
    else memcpy(reinterpret_cast<void *>(addr), msg.ptr, msg.count);
    return true;
  }


  bool  receive(MessageMemRead &msg)
  {
    unsigned addr = msg.phys;
    if (!match_bars(addr, msg.count, false))  return false;
    if (msg.count == 4)  *reinterpret_cast<unsigned *      >(msg.ptr) = *reinterpret_cast<unsigned *      >(addr);
    if (msg.count == 2)  *reinterpret_cast<unsigned short *>(msg.ptr) = *reinterpret_cast<unsigned short *>(addr);
    else memcpy(msg.ptr,  reinterpret_cast<void *>(addr), msg.count);
    return true;
  }


  DirectPciDevice(Motherboard &mb, unsigned address, unsigned hostirq) : HostPci(mb.bus_hwpcicfg), _mb(mb), _address(address), _hostirq(hostirq), _bars(), _masks()
    {
      for (unsigned i=0; i < PCI_CFG_SPACE_DWORDS; i++) _cfgspace[i] = conf_read(address | i<<2);
      read_bars();

      // disable msi
      unsigned offset = find_cap(address, 0x5);
      if (offset)
	{
	  unsigned ctrl = conf_read(address + offset);
	  Logging::printf("MSI cap @%x ctrl %x  - disabling\n", offset, ctrl);
	  ctrl &= 0xffff;
	  conf_write(address + offset, ctrl);
	}
      // and msi-x
      offset = find_cap(address, 0x11);
      if (offset)
	{
	  unsigned ctrl = conf_read(address + offset);
	  ctrl &= 0xffff;
	  conf_write(address + offset, ctrl);
	  Logging::printf("MSI-X cap @%x ctrl %x  - disabling\n", offset, ctrl);
	  conf_write(address + offset, ctrl);
	}
    }
};

PARAM(dpci,
      {
	HostPci  pci(mb.bus_hwpcicfg);
	unsigned irqline = ~0UL;
	unsigned irqpin;
	unsigned address = pci.search_device(argv[0], argv[1], argv[2], irqline, irqpin);
	if (!address)
	  Logging::panic("search_device(%lx,%lx,%lx) failed\n", argv[0], argv[1], argv[2]);
	else
	  {
	    if (argv[5] != ~0UL) irqline = argv[5];
	    Logging::printf("search_device(%lx,%lx,%lx) hostirq %x address %x \n", argv[0], argv[1], argv[2], irqline, address);
	    DirectPciDevice *dev = new DirectPciDevice(mb, address, irqline);

	    // add to PCI bus
	    MessagePciBridgeAdd msg2(argv[4], dev, &DirectPciDevice::receive_static<MessagePciCfg>);
	    if (!mb.bus_pcibridge.send(msg2, argv[3] == ~0UL ? 0 : argv[3]))
	      Logging::printf("could not add PCI device to %lx:%lx\n", argv[3], argv[4]);

	    mb.bus_ioin.add(dev, &DirectPciDevice::receive_static<MessageIOIn>);
	    mb.bus_ioout.add(dev, &DirectPciDevice::receive_static<MessageIOOut>);
	    mb.bus_memread.add(dev, &DirectPciDevice::receive_static<MessageMemRead>);
	    mb.bus_memwrite.add(dev, &DirectPciDevice::receive_static<MessageMemWrite>);
	    if (irqline != ~0UL)
	      {
		mb.bus_hostirq.add(dev, &DirectPciDevice::receive_static<MessageIrq>);
		mb.bus_irqnotify.add(dev, &DirectPciDevice::receive_static<MessageIrqNotify>);
		MessageHostOp msg3(MessageHostOp::OP_ATTACH_HOSTIRQ, irqline);
		mb.bus_hostop.send(msg3);
	      }
	  }
	Logging::printf("dpci arg done\n");
      },
      "dpci:class,subclass,instance,bus,devicefun,hostirq - makes the specified hostdevice directly accessible to the guest.",
      "Example: Use 'dpci:2,,0,,0x21,0x35' to attach the first network controller to 00:04.1 by forwarding hostirq 0x35.",
      "If class or subclass is ommited it is not compared. If the instance is ommited the last instance is used.",
      "If bus is ommited the first bus is used. If hostirq is ommited the irqline from the device is used instead.");
