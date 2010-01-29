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

  Motherboard &_mb;

  uint16_t _parent_bdf;		// BDF of Physical Function
  uint16_t _vf_bdf;		// BDF of VF on host
  uint16_t _guest_bdf;		// BDF of VF inside the guest

  // Device and Vendor ID as reported by parent's SR-IOV cap.
  uint32_t _device_vendor_id;

  unsigned _vf_no;

  enum BarType {
    BAR_32BIT,
    BAR_64BIT_LO,
    BAR_64BIT_HI,
  };

  struct {
    uint64_t base;
    uint64_t size;
    BarType  type;

  } _bars[MAX_BAR];

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
    if (in_range(addr, 0x10, MAX_BAR))
      return (addr - 0x10) >> 2;
    else
      return ~0U;
  }

  void read_bars()
  {
    for (unsigned i = 0; i < MAX_BAR; i++) {

    }
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
      if (msg.offset == 0)
	msg.value = _device_vendor_id;
      else if ((bar = in_bar(msg.offset)) < MAX_BAR) {
	switch (_bars[bar].type) {
	case BAR_64BIT_HI: msg.value = _bars[bar-1].base>>32; break;
	case BAR_32BIT:    msg.value = (uint32_t)_bars[bar].base | HostPci::BAR_TYPE_32B; break;
	case BAR_64BIT_LO: msg.value = (uint32_t)_bars[bar].base | HostPci::BAR_TYPE_64B; break;
	}
	this->msg("BAR %d -> %08x\n", bar, msg.value);
	return true;
      } else 
	msg.value = conf_read(_vf_bdf, msg.offset);
      break;
    case MessagePciConfig::TYPE_WRITE:
      if ((bar = in_bar(msg.offset)) < MAX_BAR) {
	this->msg("BAR %d <- %08x\n", bar, msg.value);
	switch (_bars[bar].type) {
	case BAR_64BIT_LO:
	case BAR_32BIT: {
	  uint64_t himask = 0xFFFFFFFFULL<<32;
	  uint64_t hi = _bars[bar].base & himask;
	  uint64_t lomask = _bars[bar].size - 1;
	  
	  _bars[bar].base = (_bars[bar].base&lomask) | (msg.value&~lomask);
	  _bars[bar].base = _bars[bar].base&~himask | hi;
	}
	  break;
	  // Assume size < 4GB
	case BAR_64BIT_HI: _bars[bar-1].base = (_bars[bar-1].base & 0xFFFFFFFFULL) | ((uint64_t)msg.value << 32); break;
	}
      } else
	conf_write(_vf_bdf, msg.offset, msg.value);
      break;
    }

    return true;
  }

  DirectVFDevice(Motherboard &mb, uint16_t parent_bdf, unsigned vf_no, uint16_t guest_bdf)
    : HostPci(mb.bus_hwpcicfg, mb.bus_hostop), _mb(mb), _parent_bdf(parent_bdf), _guest_bdf(guest_bdf), _vf_no(vf_no)
  {
    memset(_bars, 0, sizeof(_bars));

    msg("Querying parent SR-IOV capability...\n");

    unsigned sriov_cap = find_extended_cap(parent_bdf, EXTCAP_SRIOV);
    if (!sriov_cap) {
      msg("XXX Parent not SR-IOV capable. Configuration error!\n");
      return;
    }

    uint16_t numvfs = conf_read(parent_bdf, sriov_cap + 0x10) & 0xFFFF;
    if (vf_no >= numvfs) {
      msg("XXX VF%d does not exist.\n", vf_no);
      return;
    }

    // Read BARs and masks.
    for (unsigned i = 0; i < MAX_BAR; i++) {
      bool b64;
      _bars[i].base = vf_bar_base(parent_bdf, i);
      _bars[i].size = vf_bar_size(parent_bdf, i, &b64);

      assert(((_bars[i].base + _bars[i].size*numvfs) >> 32) == 0);

      _bars[i].base += _bars[i].size*vf_no;

      msg("bar[%d] -> %08llx %08llx\n", i, _bars[i].base, _bars[i].size);

      if (!b64)
	_bars[i].type = BAR_32BIT;
      else {
	_bars[i].type   = BAR_64BIT_LO;
	_bars[i+1].type = BAR_64BIT_HI;
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
	if (_mb.bus_hostop.send(amsg) && amsg.ptr) {
	  msg("MMIO %08llx -> %p\n", _bars[i].base, amsg.ptr);
	  _bars[i].base = (uintptr_t)amsg.ptr;
	  assert((_bars[i].base & (_bars[i].size-1)) == 0);
	} else {
	  msg("MMIO %08llx -> ?!?!? (Disabling BAR%d!)\n", _bars[i].base, i);
	_bars[i].base = 0;
	_bars[i].size = 0;
	}
      }
      
      if (_bars[i].type != BAR_32BIT) i++;
    }


    _device_vendor_id = conf_read(parent_bdf, sriov_cap + 0x18) & 0xFFFF0000;
    _device_vendor_id |= conf_read(parent_bdf, 0) & 0xFFFF;
    msg("Our device ID is %04x.\n", _device_vendor_id);

    unsigned vf_offset = conf_read(parent_bdf, sriov_cap + 0x14);
    unsigned vf_stride = vf_offset >> 16;
    vf_offset &= 0xFFFF;
    _vf_bdf = parent_bdf + vf_stride*vf_no + vf_offset;
    msg("VF is at %04x.\n", _vf_bdf);

    // TODO Map VFs memory
    // TODO Emulate BAR access
    // TODO IRQs

    MessageHostOp msg_assign(MessageHostOp::OP_ASSIGN_PCI, parent_bdf, _vf_bdf);
    if (!mb.bus_hostop.send(msg_assign)) {
      msg("Could not assign %04x/%04x via IO/MMU.\n", parent_bdf, _vf_bdf);
      return;
    }

    // Add device to bus
    MessagePciBridgeAdd msg(guest_bdf & 0xff, this, &DirectVFDevice::receive_static<MessagePciConfig>);
    if (!mb.bus_pcibridge.send(msg, guest_bdf >> 8)) {
      Logging::printf("Could not add PCI device to %x\n", guest_bdf);
    }
  }
};

PARAM(vfpci,
      {
	HostPci  pci(mb.bus_hwpcicfg, mb.bus_hostop);
	uint16_t parent_bdf = argv[0];
	unsigned vf_no      = argv[1];
	uint16_t guest_bdf   = argv[2];

	Logging::printf("VF %08x\n", pci.conf_read(parent_bdf, 0));
	new DirectVFDevice(mb, parent_bdf, vf_no, guest_bdf);

      },
      "vfpci:parent_bdf,vf_no,guest_bdf");

// EOF
