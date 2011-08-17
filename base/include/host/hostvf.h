/** @file
 * PCI helper functions for virtual functions.
 *
 * Copyright (C) 2009-2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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
#include "host/hostpci.h"

/**
 * A helper for PCI config space access for virtual functions.
 */
class HostVfPci : public HostPci
{
public:
  enum {
    SRIOV_VF_BAR0           = 9,

    EXTCAP_ARI              = 0x000EU,
    EXTCAP_SRIOV            = 0x0010U
  };

  /**
   * Return the base and size of a VF BAR (inside a SR-IOV capability).
   */
  unsigned long long vf_bar_base_size(unsigned bdf, unsigned vf_no, unsigned no, unsigned long long &size, bool *is64bit=0) {

    unsigned sriov_cap = find_extended_cap(bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return -1;
    size =  bar_size(bdf, sriov_cap + SRIOV_VF_BAR0 + no, is64bit);
    return  bar_base(bdf, sriov_cap + SRIOV_VF_BAR0 + no) + vf_no * size;
  }


  /** Compute BDF of a particular VF. */
  unsigned vf_bdf(unsigned parent_bdf, unsigned vf_no)
  {
    unsigned sriov_cap = find_extended_cap(parent_bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return 0;

    unsigned vf_offset = conf_read(parent_bdf, sriov_cap + 5);
    unsigned vf_stride = vf_offset >> 16;
    vf_offset &= 0xFFFF;
    return parent_bdf + vf_stride*vf_no + vf_offset;
  }

  unsigned vf_device_id(unsigned parent_bdf)
  {
    unsigned sriov_cap = find_extended_cap(parent_bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return 0;

    return (conf_read(parent_bdf, sriov_cap + 6) & 0xFFFF0000)
      | (conf_read(parent_bdf, 0) & 0xFFFF);
  }
 HostVfPci(DBus<MessageHwPciConfig> &bus_pcicfg, DBus<MessageHostOp> &bus_hostop) : HostPci(bus_pcicfg, bus_hostop) {}
};
