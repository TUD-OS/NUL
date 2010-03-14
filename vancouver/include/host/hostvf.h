/**
 * PCI helper functions for virtual functions.
 *
 * Copyright (C) 2009-2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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
#include "nul/types.h"

/**
 * A helper for PCI config space access for virtual functions.
 */
class HostVfPci : public HostPci
{
public:
  enum {
    BAR_TYPE_MASK = 0x6,
    BAR_TYPE_32B  = 0x0,
    BAR_TYPE_64B  = 0x4,

    SRIOV_VF_BAR0           = 9,

    CAP_PCI_EXPRESS         = 0x10U,
    EXTCAP_ARI              = 0x000EU,
    EXTCAP_SRIOV            = 0x0010U,
  };

  /**
   * Return the base and size of a VF BAR (inside a SR-IOV capability).
   */
  uint64 vf_bar_base_size(unsigned bdf, unsigned vf_no, unsigned no, mword &size, bool *is64bit=0) {

    unsigned sriov_cap = find_extended_cap(bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return -1;
    size =  bar_size(bdf, sriov_cap + SRIOV_VF_BAR0 + no, is64bit);
    return  bar_base(bdf, sriov_cap + SRIOV_VF_BAR0 + no) + vf_no * size;
  }

  uint64 bar_base(unsigned bdf, unsigned bar)
  {
    unsigned val = conf_read(bdf, bar);
    if ((val & BAR_IO) == BAR_IO)  return val;

    switch (val & BAR_TYPE_MASK) {
    case BAR_TYPE_32B: return val & BAR_MEM_MASK;
    case BAR_TYPE_64B: return (static_cast<unsigned long long>(conf_read(bdf, bar + 1))<<32) | (val & BAR_MEM_MASK);
    default: return ~0ULL;
    };
  }


  /**
   * Determines BAR size. You should probably disable interrupt
   * delivery from this device, while querying BAR sizes.
   */
  uint64 bar_size(unsigned bdf, unsigned bar, bool *is64bit = 0)
  {
    uint32 old = conf_read(bdf, bar);
    uint64 size = 0;

    if (is64bit) *is64bit = false;
    if ((old & BAR_IO) == 1) {

      // I/O BAR
      conf_write(bdf, bar, 0xFFFFFFFFU);
      size = ((conf_read(bdf, bar) & BAR_IO_MASK) ^ 0xFFFFU) + 1;

    } else {

      // Memory BAR
      switch (old & BAR_TYPE_MASK) {
      case BAR_TYPE_32B:
	conf_write(bdf, bar, 0xFFFFFFFFU);
	size = ((conf_read(bdf, bar) & BAR_MEM_MASK) ^ 0xFFFFFFFFU) + 1;
	break;
      case BAR_TYPE_64B: {
	if (is64bit) *is64bit = true;
	uint32 old_hi = conf_read(bdf, bar + 1);
	conf_write(bdf, bar, 0xFFFFFFFFU);
	conf_write(bdf, bar + 1, 0xFFFFFFFFU);
	uint64 bar_size = static_cast<uint64>(conf_read(bdf, bar + 1))<<32;
	bar_size = (((bar_size | conf_read(bdf, bar)) & ~0xFULL) ^ ~0ULL) + 1;
	size = bar_size;
	conf_write(bdf, bar + 1, old_hi);
	break;
      }
      default:
	// Not Supported. Return 0.
	return 0;
      }
    }

    conf_write(bdf, bar, old);
    return size;
  }


  /**
   * Read all vf bars.
   */
  void read_all_vf_bars(unsigned bdf, unsigned vf_no, mword *base, mword *size) {

    memset(base, 0, MAX_BAR*sizeof(*base));
    memset(size, 0, MAX_BAR*sizeof(*size));

    // read bars
    for (unsigned i=0; i < count_bars(bdf); i++) {
      bool is64bit;
      base[i] = vf_bar_base_size(bdf, vf_no, i, size[i], &is64bit);
      if (is64bit) i++;
    }
  }

  /**
   * Find the position of an extended PCI capability.
   */
  unsigned find_extended_cap(unsigned bdf, unsigned short id)
  {
    unsigned long header, offset;

    if ((find_cap(bdf, CAP_PCI_EXPRESS)) && (~0UL != conf_read(bdf, 0x40)))
      for (offset = 0x100, header = conf_read(bdf, offset >> 2);
	   offset != 0;
	   offset = header >> 20, header = conf_read(bdf, offset >> 2))
	if ((header & 0xFFFF) == id)
	  return offset >> 2;
    return 0;
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

  uint32 vf_device_id(unsigned parent_bdf)
  {
    unsigned sriov_cap = find_extended_cap(parent_bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return 0;

    return (conf_read(parent_bdf, sriov_cap + 6) & 0xFFFF0000)
      | (conf_read(parent_bdf, 0) & 0xFFFF);
  }
 HostVfPci(DBus<MessagePciConfig> &bus_pcicfg, DBus<MessageHostOp> &bus_hostop) : HostPci(bus_pcicfg, bus_hostop) {}
};
