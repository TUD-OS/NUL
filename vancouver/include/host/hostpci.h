/**
 * PCI helper functions for PCI drivers.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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

#include "vmm/motherboard.h"

/**
 * A helper for PCI config space access.
 */
class HostPci
{
  DBus<MessagePciConfig> &_bus_pcicfg;
  DBus<MessageHostOp>    &_bus_hostop;

 public:

  enum {
    BAR0 = 0x10,
    BAR1 = 0x14,

    MAX_BAR  = 6,
    BAR_SIZE = 4,

    SRIOV_VF_BAR0 = 0x24U,
  };

  enum {
    BAR_PREFETCH  = 0x4,

    BAR_TYPE_MASK = 0x6,
    BAR_TYPE_32B  = 0x0,
    BAR_TYPE_64B  = 0x4,
    BAR_TYPE_1MB  = 0x2,

    BAR_IO        = 0x1,
    BAR_IO_MASK   = 0xFFFFFFFCU,
    BAR_MEM_MASK  = 0xFFFFFFF0U,
  };

  enum {
    CAP_MSI                 = 0x05U,
    CAP_PCI_EXPRESS         = 0x10U,
    CAP_MSIX                = 0x11U,

    EXTCAP_ARI              = 0x000EU,
    EXTCAP_SRIOV            = 0x0010U,
  };

  unsigned conf_read(unsigned bdf, unsigned short offset)
  {
    MessagePciConfig msg(bdf, offset);
    _bus_pcicfg.send(msg, true);
    return msg.value;
  }

  void conf_write(unsigned bdf, unsigned short offset, unsigned value)
  {
    MessagePciConfig msg(bdf, offset, value);
    _bus_pcicfg.send(msg, true);
  }

  unsigned long long bar_base(unsigned bdf, unsigned bar)
  {
    unsigned val = conf_read(bdf, bar);
    if ((val & BAR_IO) == BAR_IO) {
      return val & ~3;		/* XXX Shift? */
    } else {
      switch (val & BAR_TYPE_MASK) {
      case BAR_TYPE_32B: return val & ~0xF;
      case BAR_TYPE_64B: return ((unsigned long long)conf_read(bdf, bar + 4)<<32) | (val & BAR_MEM_MASK);
      default: return ~0ULL;
      }
    };
  }


  /** Determines BAR size. You should probably disable interrupt
   * delivery from this device, while querying BAR sizes.
   */
  unsigned long long bar_size(unsigned bdf, unsigned bar, bool *is64bit = NULL)
  {
    unsigned old = conf_read(bdf, bar);
    unsigned long long size = 0;

    if (is64bit) *is64bit = false;
    if ((old & BAR_IO) == 1) {
      // I/O BAR
      conf_write(bdf, bar, 0xFFFFFFFFU);
      size = ((conf_read(bdf, bar) & BAR_IO_MASK) ^ 0xFFFFFFFFU) + 1;
    } else {
      // Memory BAR
      switch (old & BAR_TYPE_MASK) {
      case BAR_TYPE_32B:
	conf_write(bdf, bar, 0xFFFFFFFFU);
	size = ((conf_read(bdf, bar) & BAR_MEM_MASK) ^ 0xFFFFFFFFU) + 1;
	break;
      case BAR_TYPE_64B: {
	if (is64bit) *is64bit = true;
	unsigned old_hi = conf_read(bdf, bar + 4);
	conf_write(bdf, bar, 0xFFFFFFFFU);
	conf_write(bdf, bar + 4, 0xFFFFFFFFU);
	unsigned long long bar_size = ((unsigned long long)conf_read(bdf, bar + 4))<<32;
	bar_size = (((bar_size | conf_read(bdf, bar)) & ~0xFULL) ^ ~0ULL) + 1;
	size = bar_size;
	conf_write(bdf, bar + 4, old_hi);
	break;
      }
      case BAR_TYPE_1MB:
      default:
	// Not Supported. Return 0.
	;
      }
    }

    conf_write(bdf, bar, old);
    return size;
  }

  /**
   * Searches for a given device and returns the bdf of it.
   */
  unsigned search_device(unsigned theclass, unsigned subclass, unsigned instance)
  {
    for (unsigned bus = 0; bus < 256; bus++)
      {
	// skip empty busses
	if (conf_read(bus << 8, 0) == ~0u) continue;
	for (unsigned dev=0; dev < 32; dev++)
	  {
	    unsigned char maxfunc = 1;
	    for (unsigned func=0; func < maxfunc; func++)
	      {
		unsigned bdf =  (bus << 8) | (dev << 3) | func;
		unsigned value = conf_read(bdf, 0x8);
		if (value == ~0UL) continue;
		if (maxfunc == 1 && conf_read(bdf, 0xc) & 0x800000)
		  maxfunc = 8;
		if ((theclass == ~0UL || ((value >> 24) & 0xff) == theclass)
		    && (subclass == ~0UL || ((value >> 16) & 0xff) == subclass)
		    && (instance == ~0UL || !instance--))
		  return bdf;
	      }
	  }
      }
    return 0;
  }


  /**
   * Returns the gsi and enables them.
   */
  unsigned get_gsi(DBus<MessageHostOp> &bus_hostop, DBus<MessageAcpi> &bus_acpi, unsigned bdf, unsigned nr, unsigned long gsi=~0ul, bool level=false) {

    // XXX global disable MSI+MSI-X
    unsigned msix_offset = find_cap(bdf, CAP_MSIX);
    unsigned msi_offset = find_cap(bdf, CAP_MSI);
    msi_offset = 0;

    // attach to an MSI
    MessageHostOp msg1(MessageHostOp::OP_ATTACH_MSI, bdf);
    if ((msi_offset || msix_offset) && !bus_hostop.send(msg1))
      Logging::panic("could not attach to msi for bdf %x\n", bdf);


    if (msix_offset)
      {
	unsigned ctrl1 = conf_read(bdf, msix_offset + 0x4);
	unsigned long base = bar_base(bdf, ctrl1 & 0x7) + (ctrl1 & ~0x7u);

	// map the MSI-X bar
	MessageHostOp msg2(MessageHostOp::OP_ALLOC_IOMEM, base & (~0xffful), 0x1000);
	if (!bus_hostop.send(msg2) || !msg2.ptr)
	  Logging::panic("can not map MSIX bar %lx+%x", msg2.value, msg2.len);

	volatile unsigned *msix_table = (volatile unsigned *) (msg2.ptr + (base & 0xfff));
	msix_table[nr*4 + 0]  = msg1.msi_address;
	msix_table[nr*4 + 1]  = msg1.msi_address >> 32;
	msix_table[nr*4 + 2]  = msg1.msi_value;
	msix_table[nr*4 + 3] &= ~1;
	conf_write(bdf, msix_offset, 1U << 31);
	Logging::panic("MSIX cap @%x ctrl %x gsi %x\n", msix_offset, ctrl1, msg1.msi_gsi);
	return msg1.msi_gsi;
      }

    // MSI
    if (msi_offset)
      {
	unsigned ctrl = conf_read(bdf, msi_offset);
	Logging::printf("MSI cap @%x ctrl %x\n", msi_offset, ctrl);

	unsigned base = msi_offset + 4;
	conf_write(bdf, base+0, msg1.msi_address);
	conf_write(bdf, base+4, msg1.msi_address >> 32);
	if (ctrl & 0x800000) base += 4;
	conf_write(bdf, base+4, msg1.msi_value);

	// we use only a single message and enable MSIs here
	conf_write(bdf, msi_offset, (ctrl & ~0x700000) | 0x10000);
	Logging::printf("MSI %x enabled for bdf %x MSI %llx/%x\n", msg1.msi_gsi, bdf, msg1.msi_address, msg1.msi_value);
	return msg1.msi_gsi;
      }

    // normal GSIs
    MessageAcpi msg3(bdf, conf_read(bdf, 0x3c) & 0xff);
    if (gsi == ~0UL && bus_acpi.send(msg3)) gsi = msg3.gsi;

    MessageHostOp msg(MessageHostOp::OP_ATTACH_IRQ, gsi | (level ? 0x100 : 0));
    if (gsi != ~0ul && !bus_hostop.send(msg)) return ~0ul;
    return gsi;
  }



  /**
   * Finds a capability for a device.
   */
  unsigned find_cap(unsigned bdf, unsigned char id)
  {
    if ((conf_read(bdf, 4) >> 16) & 0x10 /* Capabilities supported? */)
      for (unsigned offset = conf_read(bdf, 0x34) & 0xFC;
	   (offset != 0) && (offset != 0xFC);
	   offset = (conf_read(bdf, offset) >> 8) & 0xFC)
	if ((conf_read(bdf, offset) & 0xFF) == id)
	  return offset;

    return 0;
  }

  unsigned find_extended_cap(unsigned bdf, unsigned short id)
  {
    unsigned long header, offset;

    if ((find_cap(bdf, CAP_PCI_EXPRESS)) && (~0UL != conf_read(bdf, 0x100)))
      for (offset = 0x100, header = conf_read(bdf, offset);
	   offset != 0;
	   offset = header>>20, header = conf_read(bdf, offset))
	if ((header & 0xFFFF) == id)
	  return offset;

    return 0;
  }

  /** Return the base of a VF BAR (inside a SR-IOV capability).
   */
  unsigned long long vf_bar_base(unsigned bdf, unsigned no)
  {
    unsigned sriov_cap = find_extended_cap(bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return -1;
    return bar_base(bdf, sriov_cap + SRIOV_VF_BAR0 + no*4);
  }

  /** Return the size of a VF BAR (inside a SR-IOV capability
   */
  unsigned long long vf_bar_size(unsigned bdf, unsigned no, bool *is64bit = NULL)
  {
    unsigned sriov_cap = find_extended_cap(bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return -1;
    return bar_size(bdf, sriov_cap + SRIOV_VF_BAR0 + no*4, is64bit);
  }

  /** Compute BDF of a particular VF. */
  unsigned vf_bdf(unsigned parent_bdf, unsigned vf_no)
  {
    unsigned sriov_cap = find_extended_cap(parent_bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return 0;

    unsigned vf_offset = conf_read(parent_bdf, sriov_cap + 0x14);
    unsigned vf_stride = vf_offset >> 16;
    vf_offset &= 0xFFFF;
    return parent_bdf + vf_stride*vf_no + vf_offset;
  }

  unsigned vf_device_id(unsigned parent_bdf)
  {
    unsigned sriov_cap = find_extended_cap(parent_bdf, EXTCAP_SRIOV);
    if (!sriov_cap) return 0;

    return (conf_read(parent_bdf, sriov_cap + 0x18) & 0xFFFF0000)
      | (conf_read(parent_bdf, 0) & 0xFFFF);
  }


 HostPci(DBus<MessagePciConfig> &bus_pcicfg, DBus<MessageHostOp> &bus_hostop) : _bus_pcicfg(bus_pcicfg), _bus_hostop(bus_hostop) {};
};
