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

PCIBus::PCIBus(NubusManager &manager)
  : _manager(manager), _no(0), _bridge(NULL), _devices(NULL), _buses(NULL)
{
  discover_devices();
}

PCIBus::PCIBus(PCIDevice &bridge)
  : _manager(bridge.bus()._manager), _bridge(&bridge), _devices(NULL), _buses(NULL)
{
  _no = (_bridge->conf_read(6) >> 8) & 0xFF;
  bridge.bus().add_bus(this);

  {
    uint32 memclaim = _bridge->conf_read(8);
    uint32 limit = ((memclaim >> 20)+1) << 20;
    uint32 base = memclaim<<16;
    Region bridge_claim(base, limit - base);
    _memregion.add(bridge_claim);
  }
  
  discover_devices();

  // We can only check, if we can enable ARI after devices on this
  // bus are discovered. They all have to support ARI.
  if (ari_capable()) {
    Logging::printf("bus[%x] Enabling ARI.\n", _no);
    assert(ari_enable());
    
    // It is important to enable SR-IOV in ascending order
    // (regarding functions). The SR-IOV capability of dependent
    // PFs might otherwise change, when we don't look anymore.
    
    // The list is changed while we traverse it, because
    // sriov_enable() adds the VFs. Should be no problem though,
    // because sriov_enable is a no-op for them.
    for (PCIDevice *dev = _devices; dev != NULL; dev = dev->next())
      dev->sriov_enable(dev->sriov_total_vfs());

    // XXX Perhaps enabling ARI brought us some new
    // devices. Continue device discovery.
  }
}

PCIBus *PCIBus::add_bus(PCIBus *bus)
{
  if (!_buses) _buses = bus; else _buses->append(bus);
  return bus;
}

PCIDevice *PCIBus::add_device(PCIDevice *device)
{
  if (!_devices) _devices = device; else _devices->append(device);
  return device;
}

uint32 PCIBus::conf_read(uint8 df, uint16 reg)
{
  return _manager.pci().conf_read(_no<<8 | df, reg);
}

bool PCIBus::conf_write(uint8 df, uint16 reg, uint32 val)
{
  _manager.pci().conf_write(_no<<8 | df, reg, val);
  return true;
}

bool PCIBus::ari_enabled()
{
  if (!_bridge) return false;
  uint8 express_cap = _bridge->find_cap(CAP_PCI_EXPRESS);
  return express_cap ? ((_bridge->conf_read(express_cap + 0xa) & (1<<5)) != 0) : false;
}

bool PCIBus::ari_enable()
{ 
  if (!_bridge) return false;
  if (_bridge->dclass() != CLASS_PCI_TO_PCI_BRIDGE) return false;
  uint8 express_cap = _bridge->find_cap(CAP_PCI_EXPRESS);
  if (express_cap == 0) return false;
  
  uint32 devctrl2 = _bridge->conf_read(express_cap + 0xa);
  _bridge->conf_write(express_cap + 0xa,  devctrl2 | (1<<5));
  
  assert(ari_enabled());
  return true;
}

void PCIBus::discover_devices()
{
  if (ari_enabled()) {
    Logging::printf("ARI-aware device discovery for bus %u.\n", _no);
    // ARI-aware discovery
    uint16 next = 0;
    do {
      if (device_exists(next)) {
	PCIDevice *dev = add_device(new PCIDevice(*this, next));
	next = dev->ari_next_function();
      } else {
	Logging::printf("ARI-aware discovery b0rken or weird PCI topology.\n");
	break;
      }
    } while (next != 0);
  } else {
    // Non-ARI (i.e. normal) discovery
    for (unsigned df = 0; df < 32*8; df += 8) {
      if (device_exists(df)) {
	PCIDevice *dev = add_device(new PCIDevice(*this, df));
	if (dev->multifunction())
	  for (unsigned f = 1; f < 8; f++)
	    if (device_exists(df | f))
	      add_device(new PCIDevice(*this, df | f));
      }
    }
  }
}

bool PCIBus::ari_capable()
{
  // All devices and the bridge must be ARI capable.
  if (_bridge && _bridge->ari_capable()) {
    for (PCIDevice *dev = _devices; dev != NULL; dev = dev->next())
      if (!dev->is_vf() && !dev->ari_capable())
	return false;
    return true;
  }
  return false;
}


// EOF
