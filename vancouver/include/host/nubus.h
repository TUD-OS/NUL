// -*- Mode: C++ -*-

#pragma once

#include <vmm/motherboard.h>
#include <host/hostpci.h>
#include <sys/region.h>

#include <util/ListEntry.h>
#include <cstdint>
#include <cassert>

enum {
  CLASS_PCI_TO_PCI_BRIDGE = 0x0604,

  CAP_MSI                 = 0x05U,
  CAP_PCI_EXPRESS         = 0x10U,
  
  EXTCAP_ARI              = 0x000EU,
  EXTCAP_SRIOV            = 0x0010U,

  SRIOV_VF_BAR0           = 0x24U,

  SRIOV_REG_CONTROL       = 0x8,
  SRIOV_VF_ENABLE         = 1,
  SRIOV_MSE_ENABLE        = 1U<<3,  // Memory Space Enable
  SRIOV_ARI               = (1<<4), // Only in function 0.
};

class NubusManager;
class PCIDevice;

class PCIBus : public ListEntry<PCIBus> {
protected:
  NubusManager &_manager;
  uint8_t    _no;
  PCIDevice *_bridge;

  PCIDevice *_devices;
  PCIBus    *_buses;

  RegionList<32> _memregion;

  bool device_exists(unsigned df) { return conf_read(df, 0) != ~0UL; };
  void discover_devices();

public:
  uint8_t       no() const { return _no; }
  NubusManager &manager() const { return _manager; };
  PCIBus       *add_bus(PCIBus *bus);
  PCIDevice    *add_device(PCIDevice *device);

  void add_used_region(uint64_t base, uint64_t size) {
    Region r(base, size);
    Logging::printf("bus[%u]: Removing %llx+%llx\n", _no, base, size);
    _memregion.del(r);
  }

  uint64_t alloc_mmio_window(uint64_t size) { return _memregion.alloc((unsigned long)size, 12); }

  uint32_t conf_read(uint8_t df, uint16_t reg);
  bool     conf_write(uint8_t df, uint16_t reg, uint32_t val);

  bool ari_enable();
  bool ari_enabled();
  bool ari_capable();

  PCIBus(PCIDevice &bridge);    
  // Constructor for the root bus. Only called by the manager itself.
  explicit PCIBus(NubusManager &manager);
};

class PCIDevice : public ListEntry<PCIDevice> {
protected:
  PCIBus &_bus;
  uint8_t _df;
  uint32_t _vendor;		// First word of ConfigSpace
  uint16_t _dclass;
  uint8_t _header_type;

  // Physical function (only set for VFs)
  PCIDevice *_pf;

public:
  PCIBus     &bus() const { return _bus; }
  uint32_t    vendor() const { return _vendor; }
  uint16_t    dclass() const { return _dclass; }
  bool        multifunction() const { return (_header_type & (1<<7)) != 0; }
  bool        is_vf() const { return _pf != NULL; }
  PCIDevice  *pf() const { return _pf; }
  uint16_t    bdf() const { return (_bus.no()<<8) | _df; }

  uint32_t conf_read(uint16_t reg);
  bool     conf_write(uint16_t reg, uint32_t val);

  uint8_t find_cap(uint8_t id);
  uint16_t find_extcap(uint16_t id);

  bool ari_capable();

  uint8_t ari_next_function() {
    uint16_t ari_cap = find_extcap(EXTCAP_ARI);
    if (ari_cap == 0) return 0;
    return (conf_read(ari_cap + 4) >> 8) & 0xFF;
  }

  uint16_t sriov_total_vfs() {
    uint16_t sriov_cap = find_extcap(EXTCAP_SRIOV);
    if (sriov_cap == 0) return 0;
    return conf_read(sriov_cap + 0xC) >> 16;
  }

  uint16_t sriov_device_id() {
    uint16_t sriov_cap = find_extcap(EXTCAP_SRIOV);
    if (sriov_cap == 0) return 0;
    return conf_read(sriov_cap + 0x18) >> 16;
  }

  bool sriov_enable(uint16_t vfs_to_enable);

  PCIDevice(PCIBus &bus, uint8_t df, PCIDevice *pf = NULL);
};


class NubusManager {
  friend class PCIBus;
protected:
  HostPci &_pci;
  Clock *_clock;
  PCIBus _root_bus;

public:
  void spin(unsigned ms);
  HostPci &pci() const { return _pci; }
  NubusManager(HostPci pcicfg, Clock *clock);
};

// EOF
