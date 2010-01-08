// -*- Mode: C++ -*-

#include <host/nubus.h>

bool PCIDevice::ari_capable()
{
  assert(!is_vf());
  if (_dclass == CLASS_PCI_TO_PCI_BRIDGE) {
    // Bridges are ARI capable if their Device Capabilities 2
    // register has the ARI bit set.

    uint8_t express_cap = find_cap(CAP_PCI_EXPRESS);
    if (express_cap == 0) return false;
	
    uint32_t devcap2 = read_cfg(express_cap + 0x24);
    return (devcap2 & (1<<5)) != 0;
  } else {
    // Normal devices need an ARI capability to be ARI capable.
    return find_extcap(EXTCAP_ARI) != 0;
  }
}

uint8_t PCIDevice::find_cap(uint8_t id)
{
  if ((read_cfg(4) >> 16) & 0x10 /* Capabilities supported? */) {
    for (unsigned offset = read_cfg(0x34) & 0xFC;
	 offset != 0; offset = (read_cfg(offset) >> 8) & 0xFC) {
      if ((read_cfg(offset) & 0xFF) == id)
	return offset;
    }
  }

  return 0;
}

uint16_t PCIDevice::find_extcap(uint16_t id)
{
  if (!find_cap(CAP_PCI_EXPRESS)) return 0;
  if (~0UL == read_cfg(0x100)) return 0;

  uint32_t header;
  uint16_t offset;
  for (offset = 0x100, header = read_cfg(offset);
       offset != 0;
       offset = header>>20, header = read_cfg(offset)) {
    if ((header & 0xFFFF) == id)
      return offset;
  }

  return 0;
}

bool PCIDevice::sriov_enable(uint16_t vfs_to_enable)
{
  uint16_t sriov_cap = find_extcap(EXTCAP_SRIOV);
  if (sriov_cap == 0) return false;

  uint32_t ctrl = read_cfg(sriov_cap + SRIOV_REG_CONTROL);
  if ((ctrl & SRIOV_VF_ENABLE) != 0) {
    Logging::printf("SR-IOV already enabled. This is bad.");
    return false;
  }
	  
  if (_df == 0) {
    // This is only needed for PF0 according to SR-IOV spec.
    Logging::printf("dev[%x:%02x.%x] Adapting ARI status on SR-IOV capable device.\n",
		    _bus.no(), _df>>3, _df&3);
    write_cfg(sriov_cap + SRIOV_REG_CONTROL,
	      (ctrl & ~SRIOV_ARI) | (_bus.ari_enabled() ? SRIOV_ARI : 0));
  }

  // for (unsigned off = 0; off < 0x24; off += 4) {
  //   Logging::printf("SR-IOV[%02x] %08x\n", off, read_cfg(sriov_cap + off));
  // }

  write_cfg(sriov_cap + 0x10, (read_cfg(sriov_cap + 0x10) & ~0xFFFF) | vfs_to_enable);
  write_cfg(sriov_cap + SRIOV_REG_CONTROL, SRIOV_VF_ENABLE | read_cfg(sriov_cap + SRIOV_REG_CONTROL));

  Logging::printf("dev[%x:%02x.%x] Enabled %u VFs. Wait for them to settle down.\n",
		  _bus.no(), _df>>3, _df&3, vfs_to_enable);
  _bus.manager().spin(200 /* ms */);

  // Add VFs to bus.
  unsigned vf_offset = read_cfg(sriov_cap + 0x14);
  unsigned vf_stride = vf_offset >> 16;
  vf_offset &= 0xFFFF;

  for (unsigned cur_vf = 0; cur_vf < vfs_to_enable; cur_vf++) {
    uint16_t vf_bdf = bdf() + vf_offset + vf_stride*cur_vf;
    if (vf_bdf>>8 != _bus.no()) {
      Logging::printf("VF %d not on our bus. Crap.\n", cur_vf);
      break;
    }

    _bus.add_device(new PCIDevice(_bus, vf_bdf & 0xFF, this));
  }

  return true;
}

PCIDevice::PCIDevice(PCIBus &bus, uint8_t df, PCIDevice *pf)
  : _bus(bus), _df(df), _pf(pf)
{
  _vendor = !is_vf() ? read_cfg(0) : ((this->pf()->vendor() & 0xFFFF) | 
				      (this->pf()->sriov_device_id()<<16)) ;
  _dclass = read_cfg(8) >> 16;
  if (is_vf()) assert(_dclass == pf->_dclass);

  _header_type = (read_cfg(12) >> 16) & 0xFF;
  uint8_t config_layout = _header_type & 0x3F;

  Logging::printf("dev[%x:%02x.%x] vendor %08x class %04x%s\n", _bus.no(), _df>>3, _df&7,
		  _vendor, _dclass, is_vf() ? " VF" : "");

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
    // XXX Look for driver.
    {}
  }
}

// EOF
