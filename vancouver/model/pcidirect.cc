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

#include "nul/motherboard.h"
#include "host/hostpci.h"
#include "model/pci.h"


/**
 * Directly assign a host PCI device to the guest.
 *
 * State: testing
 * Features: pcicfgspace, ioport operations, memory read/write, host irq, mem-alloc, DMA remapping
 * Missing: MSI, MSI-X
 * Documentation: PCI spec v.2.2
 */
class DirectPciDevice : public StaticReceiver<DirectPciDevice>, public HostPci
{
  Motherboard &_mb;
  unsigned _bdf;
  unsigned _hostirq;
  unsigned _cfgspace[PCI_CFG_SPACE_DWORDS];
  unsigned _bar_count;
  long long unsigned _bases[MAX_BAR];
  long long unsigned _sizes[MAX_BAR];
  const char *debug_getname() { return "DirectPciDevice"; }

  /**
   * Map the bars.
   */
  void map_bars() {
    for (unsigned i=0; i < _bar_count; i++) {

      unsigned long bar = _bases[i];
      if (!bar) continue;

      if ((bar & 1) == 1) {
	MessageHostOp msg(MessageHostOp::OP_ALLOC_IOIO_REGION, ((bar & BAR_IO_MASK) << 8) |  Cpu::bsr(_sizes[i] | 0x3));
	_mb.bus_hostop.send(msg);
      } else {

	MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, bar & ~0x1f, 1 << Cpu::bsr(((_sizes[i] - 1) | 0xfff) + 1));
	if (_mb.bus_hostop.send(msg) && msg.ptr)
	  _bases[i] = reinterpret_cast<unsigned long>(msg.ptr) + (bar & 0x10);
	else
	  Logging::panic("can not map IOMEM region %lx+%x", msg.value, msg.len);

      }
      Logging::printf("%s() bar %lx -> %llx size %llx\n", __func__, bar, _bases[i], _sizes[i]);
    }
  }

  /**
   * Check whether the guest io address matches and translate to host
   * address.
   */
  unsigned match_bars(unsigned &address, unsigned size, bool iospace)
  {
    if (iospace && address < 0x100) return false;

    COUNTER_INC("PCIDirect::match");
    // check whether io decode is disabled
    if (iospace && ~_cfgspace[1] & 1 || !iospace && ~_cfgspace[1] & 2)  return 0;
    for (unsigned i=0; i < _bar_count; i++)
      {
	unsigned  bar = _cfgspace[4 + i];
	// mask prefetch bit
	if (!iospace) bar &= ~0xc;
	if (_sizes[i] || (bar & 1) != iospace || !in_range(address, bar & ~0x3, _sizes[i] + 1 - size + 1))
	  continue;
	address = address - bar + _bases[i];
	return 4 + i;
      }
    return 0;
  }

 public:


  bool receive(MessageIOIn &msg)
  {
    unsigned old_port = msg.port;
    unsigned new_port = msg.port;
    if (!match_bars(new_port, 1 << msg.type, true))  return false;
    msg.port = new_port;
    bool res = _mb.bus_hwioin.send(msg);
    msg.port = old_port;
    return res;
  }


  bool receive(MessageIOOut &msg)
  {
    unsigned old_port = msg.port;
    unsigned new_port = msg.port;
    if (!match_bars(new_port, 1 << msg.type, true))  return false;
    msg.port = new_port;
    bool res = _mb.bus_hwioout.send(msg);
    msg.port = old_port;
    return res;
  }


  bool receive(MessagePciConfig &msg)
  {
    if (!msg.bdf)
      {
	assert(msg.offset <= PCI_CFG_SPACE_MASK);
	assert(!(msg.offset & 3));
	if (msg.type == MessagePciConfig::TYPE_READ)
	  {
	    if (in_range(msg.offset, 0x10, MAX_BAR * 4))
	      memcpy(&msg.value, reinterpret_cast<char *>(_cfgspace) + msg.offset, 4);
	    else
	      msg.value = conf_read(_bdf, msg.offset & ~0x3) >> (8 * (msg.offset & 3));

	    // disable capabilities, thus MSI is not known
	    if (msg.offset == 0x4)   msg.value &= ~0x10;
	    if (msg.offset == 0x34)  msg.value &= ~0xff;

	    // disable multi-function devices
	    if (msg.offset == 0xc)   msg.value &= ~0x800000;
	    //Logging::printf("%s:%x -- %8x,%8x\n", __PRETTY_FUNCTION__, _bdf, msg.offset, msg.value);
	    return true;
	  }
	else
	  {
	    //unsigned old = _cfgspace[msg.offset >> 2];
	    unsigned mask = ~0u;
	    if (in_range(msg.offset, 0x10, MAX_BAR * 4))  mask &= _sizes[(msg.offset - 0x10) >> 2] - 1;
	    _cfgspace[msg.offset >> 2] = _cfgspace[msg.offset >> 2] & ~mask | msg.value & mask;

	    //write through if not in the bar-range
	    if (!in_range(msg.offset, 0x10, MAX_BAR * 4))
	      conf_write(_bdf, msg.offset, _cfgspace[msg.offset >> 2]);

	    //Logging::printf("%s:%x -- %8x,%8x old %8x\n", __PRETTY_FUNCTION__, _bdf, msg.offset, _cfgspace[msg.offset >> 2], old);
	  return true;
	  }
      }
    return false;
  }


  bool receive(MessageIrq &msg)
  {
    if (msg.line != _hostirq)  return false;
    //Logging::printf("Forwarding irq message #%x  %x -> %x\n", msg.type, msg.line, _cfgspace[15] & 0xff);
    MessageIrq msg2(msg.type, _cfgspace[15] & 0xff);
    return _mb.bus_irqlines.send(msg2);
  }


  bool receive(MessageIrqNotify &msg)
  {
    unsigned irq = _cfgspace[15] & 0xff;
    if (in_range(irq, msg.baseirq, 8) && msg.mask & (1 << (irq & 0x7)))
      {
	//Logging::printf("Notify irq message #%x  %x -> %x\n", msg.mask, msg.baseirq, _hostirq);
	MessageHostOp msg2(MessageHostOp::OP_NOTIFY_IRQ, _hostirq);
	return _mb.bus_hostop.send(msg2);
      }
    return false;
  }


  bool receive(MessageMemAlloc &msg)
  {
    COUNTER_INC("PCIDirect::alloc");
    unsigned addr = msg.phys1 & ~0xfff;
    unsigned ofs = match_bars(addr, 0x1000, false);
    if (!ofs || msg.phys2 != ~0xffful)  return false;
    unsigned bar = _cfgspace[ofs];
    // prefetchable?
    if (~bar & 0x8) return false;
    *msg.ptr = reinterpret_cast<void *>(addr + (msg.phys1 & 0xfff));
    return true;

  }

  bool receive(MessageMemWrite &msg)
  {
    unsigned addr = msg.phys;
    if (!match_bars(addr, msg.count, false))  return false;
    COUNTER_INC("PCIDirect::write");
    switch (msg.count)
      {
      case 4:
	*reinterpret_cast<unsigned       *>(addr) = *reinterpret_cast<unsigned       *>(msg.ptr);
	break;
      case 2:
	*reinterpret_cast<unsigned short *>(addr) = *reinterpret_cast<unsigned short *>(msg.ptr);
	break;
      case 1:
	*reinterpret_cast<unsigned char  *>(addr) = *reinterpret_cast<unsigned char  *>(msg.ptr);
	break;
      default:
	memcpy(reinterpret_cast<void *>(addr), msg.ptr, msg.count);
      }
    return true;
  }


  bool receive(MessageMemRead &msg)
  {
    unsigned addr = msg.phys;
    if (!match_bars(addr, msg.count, false))  return false;
    COUNTER_INC("PCIDirect::read");
    switch (msg.count)
      {
      case 4:
	*reinterpret_cast<unsigned       *>(msg.ptr) = *reinterpret_cast<unsigned       *>(addr);
	break;
      case 2:
	*reinterpret_cast<unsigned short *>(msg.ptr) = *reinterpret_cast<unsigned short *>(addr);
	break;
      case 1:
	*reinterpret_cast<unsigned char  *>(msg.ptr) = *reinterpret_cast<unsigned char  *>(addr);
	break;
      default:
	memcpy(msg.ptr, reinterpret_cast<void *>(addr), msg.count);
      }
    return true;
  }

  bool  receive(MessageMemMap &msg)
  {
    for (unsigned i=0; i < _bar_count; i++)
      {
	unsigned  bar = _cfgspace[4 + i];
	if (_sizes[i] || (bar & 1) || !in_range(msg.phys, bar & ~0x3, _sizes[i]))
	  continue;
	msg.ptr  = reinterpret_cast<char *>(_bases[i] & ~0xfff);
	msg.count = _sizes[i];
	Logging::printf(" MAP %lx+%x from %p\n", msg.phys, msg.count, msg.ptr);
	return true;
      }
    return false;
  }

  DirectPciDevice(Motherboard &mb, unsigned bdf, unsigned dstbdf, unsigned long long *bases, unsigned long long *sizes, unsigned didvid = 0)
    : HostPci(mb.bus_hwpcicfg, mb.bus_hostop), _mb(mb), _bdf(bdf), _bar_count(count_bars(_bdf))
  {

    memcpy(_bases, bases, sizeof(_bases));
    memcpy(_sizes, sizes, sizeof(_sizes));

    for (unsigned i=0; i < PCI_CFG_SPACE_DWORDS; i++) _cfgspace[i] = conf_read(_bdf, i<<2);
    _hostirq = get_gsi(mb.bus_hostop, mb.bus_acpi, bdf, 0, true);

    dstbdf = (dstbdf == 0) ? bdf : PciHelper::find_free_bdf(mb.bus_pcicfg, dstbdf);
    mb.bus_pcicfg.add(this, &DirectPciDevice::receive_static<MessagePciConfig>, dstbdf);
    mb.bus_ioin.add(this, &DirectPciDevice::receive_static<MessageIOIn>);
    mb.bus_ioout.add(this, &DirectPciDevice::receive_static<MessageIOOut>);
    mb.bus_memread.add(this, &DirectPciDevice::receive_static<MessageMemRead>);
    mb.bus_memwrite.add(this, &DirectPciDevice::receive_static<MessageMemWrite>);
    mb.bus_memmap.add(this, &DirectPciDevice::receive_static<MessageMemMap>);
    mb.bus_hostirq.add(this, &DirectPciDevice::receive_static<MessageIrq>);
    mb.bus_irqnotify.add(this, &DirectPciDevice::receive_static<MessageIrqNotify>);
  }
};


PARAM(dpci,
      {
	HostPci  pci(mb.bus_hwpcicfg, mb.bus_hostop);
	unsigned bdf = pci.search_device(argv[0], argv[1], argv[2]);

	check0(!bdf, "search_device(%lx,%lx,%lx) failed", argv[0], argv[1], argv[2]);
	Logging::printf("search_device(%lx,%lx,%lx) bdf %x \n", argv[0], argv[1], argv[2], bdf);

	MessageHostOp msg4(MessageHostOp::OP_ASSIGN_PCI, bdf);
	check0(!mb.bus_hostop.send(msg4), "DPCI: could not directly assign %x via iommu", bdf);


	unsigned long long bases[HostPci::MAX_BAR];
	unsigned long long sizes[HostPci::MAX_BAR];
	pci.read_all_bars(bdf, bases, sizes);
	new DirectPciDevice(mb, bdf, argv[3], bases, sizes);
      },
      "dpci:class,subclass,instance,bdf - makes the specified hostdevice directly accessible to the guest.",
      "Example: Use 'dpci:2,,0,0x21' to attach the first network controller to 00:04.1.",
      "If class or subclass is ommited it is not compared. If the instance is ommited the last instance is used.",
      "If bdf is zero the very same bdf as in the host is used, if it is ommited a free bdf is searched.");


#include "host/hostvf.h"

PARAM(vfpci,
      {
	HostVfPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
	unsigned parent_bdf = argv[0];
	unsigned vf_no      = argv[1];

	// Compute BDF of VF
	unsigned vf_bdf = pci.vf_bdf(parent_bdf, vf_no);
	check0(!vf_bdf, "XXX VF%d does not exist in parent %x.", vf_no, parent_bdf);
	Logging::printf("VF is at %04x.\n", vf_bdf);

	// Retrieve device and vendor IDs
	unsigned didvid = pci.vf_device_id(parent_bdf);
	Logging::printf("Our device ID is %04x.\n", didvid);

	// Put device into our address space
	MessageHostOp msg_assign(MessageHostOp::OP_ASSIGN_PCI, parent_bdf, vf_bdf);
	check0(!mb.bus_hostop.send(msg_assign), "Could not assign %04x/%04x via IO/MMU.", parent_bdf, vf_bdf);

	unsigned long long bases[HostPci::MAX_BAR];
	unsigned long long sizes[HostPci::MAX_BAR];
	pci.read_all_vf_bars(parent_bdf, vf_no, bases, sizes);

	new DirectPciDevice(mb, vf_bdf, argv[2], bases, sizes, didvid);
      },
      "vfpci:parent_bdf,vf_no,guest_bdf - directly assign a given virtual function to the guest.",
      "if no guest_bdf is given, a free one is searched.");
