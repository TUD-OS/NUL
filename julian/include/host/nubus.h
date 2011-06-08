// -*- Mode: C++ -*-
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

#pragma once

#include <nul/motherboard.h>
#include <host/hostvf.h>
#include <nul/region.h>
#include <nul/types.h>

template <class T>
class ListEntry {

private:
  T *_prev;
  T *_next;

public:
  T *prev() const { return _prev; }
  T *next() const { return _next; }

  T *first() {
    T *first;
    for (first = static_cast<T *>(this); first->_prev; first = first->_prev)
      ;
    return prev;
  }

  T *last() {
    T *last;
    for (last = static_cast<T *>(this); last->_next; last = last->_next)
      ;
    return last;
  }

  void append(T *i) {
    T *la = last();
    la->_next = i;
    i->_prev = la;
  }

  explicit ListEntry() : _prev(0), _next(0) {}
};




enum {
  CLASS_PCI_TO_PCI_BRIDGE = 0x0604,

  CAP_MSI                 = 0x05U,
  CAP_PCI_EXPRESS         = 0x10U,

  EXTCAP_ARI              = 0x000EU,
  EXTCAP_SRIOV            = 0x0010U,

  SRIOV_VF_BAR0           = 0x9U,

  SRIOV_REG_CONTROL       = 0x2,
  SRIOV_VF_ENABLE         = 1,
  SRIOV_MSE_ENABLE        = 1U<<3,  // Memory Space Enable
  SRIOV_ARI               = (1<<4), // Only in function 0.
};

class NubusManager;
class PCIDevice;

class PCIBus : public ListEntry<PCIBus> {
protected:
  NubusManager &_manager;
  uint8    _no;
  PCIDevice *_bridge;

  PCIDevice *_devices;
  PCIBus    *_buses;

  RegionList<32> _memregion;

  bool device_exists(unsigned df) { return conf_read(df, 0) != ~0UL; };
  void discover_devices();

public:
  uint8       no() const { return _no; }
  NubusManager &manager() const { return _manager; };
  PCIBus       *add_bus(PCIBus *bus);
  PCIDevice    *add_device(PCIDevice *device);

  void add_used_region(uint64 base, uint64 size) {
    Region r(base, size);
    _memregion.del(r);
  }

  uint64 alloc_mmio_window(uint64 size) { return _memregion.alloc(size, 12); }

  uint32 conf_read(uint8 df, uint16 reg);
  bool     conf_write(uint8 df, uint16 reg, uint32 val);

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
  uint8 _df;
  uint32 _vendor;		// First word of ConfigSpace
  uint16 _dclass;
  uint8 _header_type;

  // Physical function (only set for VFs)
  PCIDevice *_pf;

public:
  PCIBus     &bus() const { return _bus; }
  uint32    vendor() const { return _vendor; }
  uint16    dclass() const { return _dclass; }
  bool        multifunction() const { return (_header_type & (1<<7)) != 0; }
  bool        is_vf() const { return _pf != NULL; }
  PCIDevice  *pf() const { return _pf; }
  uint16    bdf() const { return (_bus.no()<<8) | _df; }

  uint32 conf_read(uint16 reg);
  bool     conf_write(uint16 reg, uint32 val);

  uint8 find_cap(uint8 id);
  uint16 find_extcap(uint16 id);

  bool ari_capable();

  uint8 ari_next_function() {
    uint16 ari_cap = find_extcap(EXTCAP_ARI);
    if (ari_cap == 0) return 0;
    return (conf_read(ari_cap + 1) >> 8) & 0xFF;
  }

  uint16 sriov_total_vfs() {
    uint16 sriov_cap = find_extcap(EXTCAP_SRIOV);
    if (sriov_cap == 0) return 0;
    return conf_read(sriov_cap + 3) >> 16;
  }

  uint16 sriov_device_id() {
    uint16 sriov_cap = find_extcap(EXTCAP_SRIOV);
    if (sriov_cap == 0) return 0;
    return conf_read(sriov_cap + 6) >> 16;
  }

  bool sriov_enable(uint16 vfs_to_enable);

  PCIDevice(PCIBus &bus, uint8 df, PCIDevice *pf = NULL);
};


class NubusManager {
  friend class PCIBus;
protected:
  HostVfPci &_pci;
  Clock *_clock;
  PCIBus _root_bus;

public:
  void spin(unsigned ms);
  HostVfPci &pci() const { return _pci; }
  NubusManager(HostVfPci pcicfg, Clock *clock);
};

// EOF
