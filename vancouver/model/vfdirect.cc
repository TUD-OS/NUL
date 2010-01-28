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

  uint32_t _bars[MAX_BAR];

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

 public:

  bool receive(MessagePciConfig &msg)
  {
    this->msg("Access to %04x %08x\n", msg.bdf, msg.offset);
    if (msg.bdf == 0) {
      	assert(msg.offset <= PCI_CFG_SPACE_MASK);
	assert(!(msg.offset & 3));
	if (msg.type == MessagePciConfig::TYPE_READ) {
	  if (msg.offset == 0)
	    msg.value = _device_vendor_id;
	  else
	    msg.value = conf_read(_vf_bdf, msg.offset);

	  return true;
	}
    } else {

    }
    return false;
  }

  DirectVFDevice(Motherboard &mb, uint16_t parent_bdf, unsigned vf_no, uint16_t guest_bdf)
    : HostPci(mb.bus_hwpcicfg), _mb(mb), _parent_bdf(parent_bdf), _guest_bdf(guest_bdf), _vf_no(vf_no)
  {
    msg("Querying parent SR-IOV capability...\n");

    unsigned sriov_cap = find_extended_cap(parent_bdf, EXTCAP_SRIOV);
    if (!sriov_cap) {
      msg("XXX Parent not SR-IOV capable. Configuration error!\n");
      return;
    }

    if ((conf_read(parent_bdf, sriov_cap + 0x10) & 0xFFFF) <= vf_no) {
      msg("XXX VF%d does not exist.\n", vf_no);
      return;
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
	HostPci  pci(mb.bus_hwpcicfg);
	uint16_t parent_bdf = argv[0];
	unsigned vf_no      = argv[1];
	uint16_t guest_bdf   = argv[2];

	Logging::printf("VF %08x\n", pci.conf_read(parent_bdf, 0));
	new DirectVFDevice(mb, parent_bdf, vf_no, guest_bdf);

      },
      "vfpci:parent_bdf,vf_no,guest_bdf");

// EOF
