/** @file
 * PCI bus handling
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include <host/nubus.h>

uint32 PCIDevice::conf_read(uint16 reg)
{
  return _bus.conf_read(_df, reg);
}

bool PCIDevice::conf_write(uint16 reg, uint32 val)
{
  return _bus.conf_write(_df, reg, val);
}

bool PCIDevice::ari_capable()
{
  assert(!is_vf());
  if (_dclass == CLASS_PCI_TO_PCI_BRIDGE) {
    // Bridges are ARI capable if their Device Capabilities 2
    // register has the ARI bit set.

    uint8 express_cap = find_cap(CAP_PCI_EXPRESS);
    if (express_cap == 0) return false;
	
    uint32 devcap2 = conf_read(express_cap + 9);
    return (devcap2 & (1<<5)) != 0;
  } else {
    // Normal devices need an ARI capability to be ARI capable.
    return find_extcap(EXTCAP_ARI) != 0;
  }
}

uint8 PCIDevice::find_cap(uint8 id)
{
  return _bus.manager().pci().find_cap(bdf(), id);
}

uint16 PCIDevice::find_extcap(uint16 id)
{
  return _bus.manager().pci().find_extended_cap(bdf(), id);
}

bool PCIDevice::sriov_enable(uint16 vfs_to_enable)
{
  uint16 sriov_cap = find_extcap(EXTCAP_SRIOV);
  if (sriov_cap == 0) return false;

  uint32 ctrl = conf_read(sriov_cap + SRIOV_REG_CONTROL);
  if ((ctrl & SRIOV_VF_ENABLE) != 0) {
    Logging::printf("SR-IOV already enabled. This is bad.");
    return false;
  }
	  
  if (_df == 0) {
    // This is only needed for PF0 according to SR-IOV spec.
    Logging::printf("dev[%x:%02x.%x] Adapting ARI status on SR-IOV capable device.\n",
		    _bus.no(), _df>>3, _df&3);
    conf_write(sriov_cap + SRIOV_REG_CONTROL,
	      (ctrl & ~SRIOV_ARI) | (_bus.ari_enabled() ? SRIOV_ARI : 0));
  }

  // for (unsigned off = 0; off < 0x24; off += 4) {
  //   Logging::printf("SR-IOV[%02x] %08x\n", off, conf_read(sriov_cap + off));
  // }

  conf_write(sriov_cap + 4, (conf_read(sriov_cap + 4) & ~0xFFFF) | vfs_to_enable);
  conf_write(sriov_cap + SRIOV_REG_CONTROL, SRIOV_VF_ENABLE | conf_read(sriov_cap + SRIOV_REG_CONTROL));

  Logging::printf("dev[%x:%02x.%x] Enabled %u VFs. Wait for them to settle down.\n",
		  _bus.no(), _df>>3, _df&3, vfs_to_enable);
  _bus.manager().spin(200 /* ms */);

  // Scan VF BARs
  for (unsigned cur_vfbar = 0; cur_vfbar < 6; cur_vfbar++) {
    unsigned vfbar_addr = sriov_cap + SRIOV_VF_BAR0 + cur_vfbar;
    uint32 bar_lo = conf_read(vfbar_addr);

    bool is64bit;
    uint64 base = _bus.manager().pci().bar_base(bdf(), vfbar_addr);
    uint64 size = _bus.manager().pci().bar_size(bdf(), vfbar_addr, &is64bit);

    if ((base == 0) && (size != 0)) {
      Logging::printf("VF BAR%d: %08x base %08llx size %08llx (*%d VFs)\n",
		      cur_vfbar, bar_lo, base, size, vfs_to_enable);
      uint64 base = _bus.alloc_mmio_window(size*vfs_to_enable);

      if (base != 0) {
	Logging::printf("BAR allocated at %llx.\n", base);
	uint32 cmd = conf_read(1);
	conf_write(1, cmd & ~2);
	conf_write(vfbar_addr, base);
	if (is64bit) conf_write(vfbar_addr+1, base>>32);
	conf_write(1, cmd);
      }
    }

    if (is64bit) cur_vfbar++;
  }
  
  // Enable Memory Space access
  conf_write(sriov_cap + SRIOV_REG_CONTROL, SRIOV_MSE_ENABLE | conf_read(sriov_cap + SRIOV_REG_CONTROL));
  
  // Add VFs to bus.
  unsigned vf_offset = conf_read(sriov_cap + 5);
  unsigned vf_stride = vf_offset >> 16;
  vf_offset &= 0xFFFF;

  for (unsigned cur_vf = 0; cur_vf < vfs_to_enable; cur_vf++) {
    uint16 vf_bdf = bdf() + vf_offset + vf_stride*cur_vf;
    if (vf_bdf>>8 != _bus.no()) {
      Logging::printf("VF %d not on our bus. Crap.\n", cur_vf);
      break;
    }

    _bus.add_device(new PCIDevice(_bus, vf_bdf & 0xFF, this));
  }

  return true;
}

PCIDevice::PCIDevice(PCIBus &bus, uint8 df, PCIDevice *pf)
  : _bus(bus), _df(df), _pf(pf)
{
  _vendor = !is_vf() ? conf_read(0) : ((this->pf()->vendor() & 0xFFFF) | 
				       (this->pf()->sriov_device_id()<<16)) ;
  _dclass = conf_read(2) >> 16;
  if (is_vf()) assert(_dclass == pf->_dclass);

  _header_type = (conf_read(3) >> 16) & 0xFF;
  uint8 config_layout = _header_type & 0x3F;

  // Logging::printf("dev[%x:%02x.%x] vendor %08x class %04x%s\n", _bus.no(), _df>>3, _df&7,
  // 		  _vendor, _dclass, is_vf() ? " VF" : "");

  switch (_dclass) {
  case CLASS_PCI_TO_PCI_BRIDGE: {
    if (config_layout != 1) {
      Logging::printf("Invalid layout %x for PCI-to-PCI bridge.\n", config_layout);
      break;
    }
    new PCIBus(*this);
    break;
  }
  default:
    if (is_vf()) break;		// VF BARs are already accounted for.
    // Not a bridge
    for (unsigned bar_i = 0; bar_i < 6; bar_i++) {
      bool is64bit;
      unsigned bar_addr = bar_i + HostPci::BAR0;
      if ((conf_read(bar_addr) & 1) == 1) continue; // I/O BAR
      uint64 base = _bus.manager().pci().bar_base(bdf(), bar_addr);

      uint32 stscmd = conf_read(1);
      conf_write(1, (stscmd & ~3) | (1<<10)); // Disable IO/MEM/IRQ
      uint64 bar_size = _bus.manager().pci().bar_size(bdf(), bar_addr, &is64bit);
      conf_write(1, stscmd);

      if (bar_size != 0) _bus.add_used_region(base, bar_size);

      if (is64bit) bar_i++;
    }
  }
}

// EOF
