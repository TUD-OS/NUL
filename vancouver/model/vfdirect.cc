/**
 * Directly-assigned SR-IOV virtual function.
 *
 * Copyright (C) 2007-2009, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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
#include "models/pci.h"

#include <cstdint>


class DirectVFDevice : public StaticReceiver<DirectVFDevice>, public HostPci
{
  const static unsigned MAX_BAR = 6;

  uint16_t _parent_bdf;		// BDF of Physical Function
  uint16_t _vf_bdf;		// BDF of VF on host
  uint32_t _didvid;             // Device and Vendor ID as reported by parent's SR-IOV cap.

  unsigned _vf_no;

  struct {
    uint32_t base;
    uint32_t size;

    void *ptr;
  } _bars[MAX_BAR];

  // for IRQ injection
  DBus<MessageIrq> &_bus_irqlines;

  // MSI handling

  unsigned _msi_cap;

  // MSI-X handling

  unsigned _msix_cap;		// Offset of MSI-X capability
  unsigned _msix_bir;		// Which BAR contains MSI-X registers?
  unsigned _msix_table_offset;
  unsigned _msix_pba_offset;
  uint16_t _msix_table_size;

  struct msix_table_entry {
    union {
      uint64_t guest_msg_addr;
      uint32_t guest_msg_addr_w[2];
    };
    uint32_t guest_msg_data;

    unsigned host_irq;
  };

  struct msix_table_entry *_msix_table;
  volatile uint32_t *_host_msix_table;

  const char *debug_getname() { return "DirectVFDevice"; }


  __attribute__ ((format (printf, 2, 3)))
  void msg(const char *msg, ...)
  {
    va_list ap;
    va_start(ap, msg);
    Logging::printf("VFPCI %02x/%d: ", _parent_bdf, _vf_no);
    Logging::vprintf(msg, ap);
    va_end(ap);
  }

  unsigned in_bar(unsigned addr)
  {
    if (in_range(addr, 0x10, MAX_BAR*4))
      return (addr - 0x10) >> 2;
    else
      return ~0U;
  }

 public:

  bool receive(MessagePciConfig &msg)
  {
    if (msg.bdf != 0) return false;
    assert(msg.offset <= PCI_CFG_SPACE_MASK);
    assert(!(msg.offset & 3));

    unsigned bar;

    switch (msg.type) {
    case MessagePciConfig::TYPE_READ:
      if (msg.offset == 0) {
	msg.value = _didvid;
      } else if (_msi_cap && (msg.offset == _msi_cap)) {
	// Disable cap by setting its type to 0xFF, but keep the
	// linked list intact.
	// XXX Protect Message Address and Data fields.
	msg.value = conf_read(_vf_bdf, msg.offset);
	msg.value |= 0xFF;
	return true;
      } else if ((bar = in_bar(msg.offset)) < MAX_BAR)
	{
	  msg.value = (uint32_t)_bars[bar].base;
	  //this->msg("BAR %d -> %08x\n", bar, msg.value);
	  return true;
	} else
	msg.value = conf_read(_vf_bdf, msg.offset);
      break;
    case MessagePciConfig::TYPE_WRITE:
      if ((bar = in_bar(msg.offset)) < MAX_BAR)
	//this->msg("BAR %d <- %08x\n", bar, msg.value);
	_bars[bar].base = msg.value & ~(_bars[bar].size - 1);
      else
	conf_write(_vf_bdf, msg.offset, msg.value);
      break;
    }

    return true;
  }

  bool receive(MessageIrq &msg)
  {
    // Which MSI-X vector was triggered?
    unsigned msix_vector = ~0UL;
    for (unsigned i = 0; i < _msix_table_size; i++)
      if (_msix_table[i].host_irq == msg.line) {
	msix_vector = i;
	break;
      }
    if (msix_vector == ~0UL)
      return false;

    this->msg("MSI-X IRQ%d! Inject %d\n", msix_vector, _msix_table[msix_vector].guest_msg_data & 0xFF);
    MessageIrq imsg(msg.type, _msix_table[msix_vector].guest_msg_data & 0xFF);
    _bus_irqlines.send(imsg);

    return true;
  }

  bool receive(MessageMemMap &msg)
  {
    for (unsigned i = 0; i < MAX_BAR; i++) {
      if ((_bars[i].size == 0) ||
	  // Don't map MSI-X memory!
	  (_msix_cap && (i == _msix_bir)) ||
	  !in_range(msg.phys, _bars[i].base, _bars[i].size))
	continue;

      // Size and phys are always page-aligned, because of the system
      // page size setting in the parent's SR-IOV capability.
      msg.count = _bars[i].size;
      msg.phys  = _bars[i].base;

      msg.ptr = _bars[i].ptr;
      assert((0xFFF & (uintptr_t)msg.ptr) == 0);

      this->msg("Map phys %lx+%x from %p\n", msg.phys, msg.count, msg.ptr);
      return true;
    }
    return false;
  }

  enum {
    NO_MATCH = ~0U,
  };

  unsigned in_msix_bar(uintptr_t ptr)
  {
    if (_msix_cap && in_range(ptr, _bars[_msix_bir].base, _bars[_msix_bir].size))
      return ptr - _bars[_msix_bir].base;
    else
      return NO_MATCH;
  }

  bool receive(MessageMemRead &rmsg)
  {
    unsigned offset;
    if ((offset = in_msix_bar(rmsg.phys)) == NO_MATCH) return false;

    unsigned entry = offset / 16;

    switch (rmsg.count) {
    case 4:
      //msg("READ  DW MSI-X %x: \n", offset);
      if ((offset % 4) != 0) {
	msg(" Unaligned!\n");
	return false;
      }
      switch (offset % 16) {
      case 0:
	*((uint32_t *)rmsg.ptr) = _msix_table[entry].guest_msg_addr_w[0]; break;
      case 4:
	*((uint32_t *)rmsg.ptr) = _msix_table[entry].guest_msg_addr_w[1]; break;
      case 8:
	*((uint32_t *)rmsg.ptr) = _msix_table[entry].guest_msg_data; break;
      case 12:
	// Vector control reads right through.
	*((uint32_t *)rmsg.ptr) = _host_msix_table[entry*4 + 3];
	break;
      };
      //msg(" -> %08x\n", *((uint32_t *)rmsg.ptr));
      break;
    case 8:
      msg("READ QW  MSI-X %x: \n", offset);
      if ((offset % 8) != 0) {
	msg(" Unaligned!\n");
	return false;
      }

      msg("XXX Implement me\n");
      return false;
      break;
    default:
      msg("XXX Only 32/64-bit reads in MSI-X table allowed! (read %08lx %d)\n",
	  rmsg.phys, rmsg.count);
      return false;
    };

    return true;
  }

  bool receive(MessageMemWrite &rmsg)
  {
    unsigned offset;
    if ((offset = in_msix_bar(rmsg.phys)) == NO_MATCH) return false;

    unsigned entry = offset / 16;

    switch (rmsg.count) {
    case 4:
      //msg("WRITE DW MSI-X %x: %x\n", offset, *((uint32_t *)rmsg.ptr));
      if ((offset % 4) != 0) {
	msg(" Unaligned!\n");
	return false;
      }
      switch (offset % 16) {
      case 0:
	_msix_table[entry].guest_msg_addr_w[0] = *((uint32_t *)rmsg.ptr);
	break;
      case 4:
	_msix_table[entry].guest_msg_addr_w[1] = *((uint32_t *)rmsg.ptr);
	break;
      case 8:
	_msix_table[entry].guest_msg_data = *((uint32_t *)rmsg.ptr);
	break;
      case 12:
	// Vector control writes right through.
	_host_msix_table[entry*4 + 3] = *((uint32_t *)rmsg.ptr);
      };
      break;
    case 8:
      msg("WRITE QW MSI-X %x: \n", offset);
      if ((offset % 8) != 0) {
	msg(" Unaligned!\n");
	return false;
      }

      msg("XXX Implement me\n");
      return false;
      break;
    default:
      msg("XXX Only 32/64-bit writes in MSI-X table allowed! (read %08lx %d)\n",
	  rmsg.phys, rmsg.count);
      return false;
    };

    return true;;
  }

  DirectVFDevice(Motherboard &mb, uint16_t parent_bdf, unsigned vf_bdf, uint32_t didvid, unsigned vf_no)
    : HostPci(mb.bus_hwpcicfg, mb.bus_hostop), _parent_bdf(parent_bdf), _vf_bdf(vf_bdf), _didvid(didvid), _vf_no(vf_no), _bars(), _bus_irqlines(mb.bus_irqlines)
  {
    // Read BARs and masks.
    for (unsigned i = 0; i < MAX_BAR; i++) {
      bool b64;
      _bars[i].base = vf_bar_base(parent_bdf, i);
      _bars[i].size = vf_bar_size(parent_bdf, i, &b64);
      _bars[i].base += _bars[i].size*vf_no;

      msg("bar[%d] -> %08x %08x\n", i, _bars[i].base, _bars[i].size);

      if (b64) {
	_bars[i+1].base = 0;	// Stored in previous array element.
	_bars[i+1].size = 0;	// Dito.
	i += 1;
      }
    }

    // Allocate MMIO regions

    for (unsigned i = 0; i < MAX_BAR; i++) {
      if (_bars[i].size != 0) {
	MessageHostOp amsg(MessageHostOp::OP_ALLOC_IOMEM,
			   _bars[i].base, _bars[i].size);
	if (mb.bus_hostop.send(amsg) && amsg.ptr) {
	  msg("MMIO %08x -> %p\n", _bars[i].base, amsg.ptr);
	  _bars[i].ptr = amsg.ptr;
	} else {
	  msg("MMIO %08x -> ?!?!? (Disabling BAR%d!)\n", _bars[i].base, i);
	  _bars[i].base = 0;
	  _bars[i].size = 0;
	}
      }
    }

    // IRQ handling (only MSI-X for now)
    _msi_cap = find_cap(_vf_bdf, CAP_MSI);
    if (_msi_cap) {
      Logging::printf("XXX MSI capability found. MSI support will be disabled!\n");
    }

    _msix_cap = find_cap(_vf_bdf, CAP_MSIX);
    if (_msix_cap) {
      // XXX PBA and table must share the same BAR for now.
      assert((conf_read(_vf_bdf, _msix_cap + 0x4)&3) == (conf_read(_vf_bdf, _msix_cap + 0x8)&3));

      _msix_table_size   = 1+ (conf_read(_vf_bdf, _msix_cap) >> 16) & ((1<<11)-1);
      _msix_pba_offset   = conf_read(_vf_bdf, _msix_cap + 0x8) & ~3;
      _msix_table_offset = conf_read(_vf_bdf, _msix_cap + 0x4);
      _msix_bir = _msix_table_offset & 3;
      _msix_table_offset &= ~3;

      msg("Allocated MSI-X table with %d elements.\n", _msix_table_size);
      _msix_table = (struct msix_table_entry *)calloc(_msix_table_size, sizeof(struct msix_table_entry));

      for (unsigned i = 0; i < _msix_table_size; i++) {
	unsigned gsi = get_gsi(_vf_bdf, ~0UL);
	msg("Host IRQ%d -> MSI-X IRQ%d\n", gsi, i);
	_msix_table[i].host_irq = gsi;
	MessageHostOp imsg(MessageHostOp::OP_ATTACH_HOSTIRQ, gsi);
	mb.bus_hostop.send(imsg);
      }

      // init host msix table
      _host_msix_table = (volatile uint32_t *)(_msix_table_offset + (char *)_bars[_msix_bir].ptr);
      for (unsigned i = 0; i < _msix_table_size; i++) {
	_host_msix_table[i*4 + 0] = 0xFEE00000; // CPU?
	_host_msix_table[i*4 + 1] = 0;
	_host_msix_table[i*4 + 2] = _msix_table[i].host_irq + 0x20;
      }
    }
  }
};

PARAM(vfpci,
      {
	HostPci  pci(mb.bus_hwpcicfg, mb.bus_hostop);
	uint16_t parent_bdf = argv[0];
	unsigned vf_no      = argv[1];
	uint16_t guest_bdf  = PciHelper::find_free_bdf(mb.bus_pcicfg, argv[2]);

	Logging::printf("VF %08x\n", pci.conf_read(parent_bdf, 0));
	for (unsigned i = 0x100; i < 0x170; i += 8)
	  Logging::printf("mmcfg[%04x] %08x %08x\n", i, pci.conf_read(parent_bdf, i), pci.conf_read(parent_bdf, i+4));

	Logging::printf("Querying parent SR-IOV capability...\n");
	unsigned sriov_cap = pci.find_extended_cap(parent_bdf, pci.EXTCAP_SRIOV);
	if (!sriov_cap) {
	  Logging::printf("XXX Parent not SR-IOV capable. Configuration error!\n");
	  return;
	}

	uint16_t numvfs = pci.conf_read(parent_bdf, sriov_cap + 0x10) & 0xFFFF;
	if (vf_no >= numvfs) {
	  Logging::printf("XXX VF%d does not exist.\n", vf_no);
	  return;
	}

	// Retrieve device and vendor IDs
	unsigned didvid = pci.conf_read(parent_bdf, sriov_cap + 0x18) & 0xFFFF0000;
	didvid |= pci.conf_read(parent_bdf, 0) & 0xFFFF;
	Logging::printf("Our device ID is %04x.\n", didvid);

	// Compute BDF of VF
	unsigned vf_offset = pci.conf_read(parent_bdf, sriov_cap + 0x14);
	unsigned vf_stride = vf_offset >> 16;
	vf_offset &= 0xFFFF;
	unsigned vf_bdf = parent_bdf + vf_stride*vf_no + vf_offset;
	Logging::printf("VF is at %04x.\n", vf_bdf);

	// Put device into our address space
	MessageHostOp msg_assign(MessageHostOp::OP_ASSIGN_PCI, parent_bdf, vf_bdf);
	if (!mb.bus_hostop.send(msg_assign)) {
	  Logging::printf("Could not assign %04x/%04x via IO/MMU.\n", parent_bdf, vf_bdf);
	  return;
	}

	Device * dev = new DirectVFDevice(mb, parent_bdf, vf_bdf, didvid, vf_no);

	// We want to map memory into VM space and intercept some accesses.
	mb.bus_memmap.add(dev, &DirectVFDevice::receive_static<MessageMemMap>);
	mb.bus_memread.add(dev, &DirectVFDevice::receive_static<MessageMemRead>);
	mb.bus_memwrite.add(dev, &DirectVFDevice::receive_static<MessageMemWrite>);
	mb.bus_hostirq.add(dev, &DirectVFDevice::receive_static<MessageIrq>);

	// Add device to virtual PCI bus
	mb.bus_pcicfg.add(dev, &DirectVFDevice::receive_static<MessagePciConfig>, guest_bdf);

      },
      "vfpci:parent_bdf,vf_no,guest_bdf",
      "if no guest_bdf is given, a free one is searched.");

// EOF
