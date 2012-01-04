/** @file
 * PCI helper functions for PCI drivers.
 *
 * Copyright (C) 2009-2010, Bernhard Kauer <bk@vmmon.org>
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

/**
 * A helper for PCI config space access.
 */
class HostPci
{
  DBus<MessageHwPciConfig> &_bus_pcicfg;
  DBus<MessageHostOp> 	   &_bus_hostop;

 public:

  enum {
    BAR0          = 4,
    MAX_BAR       = 6,

    BAR_TYPE_MASK = 0x6,
    BAR_TYPE_32B  = 0x0,
    BAR_TYPE_64B  = 0x4,

    BAR_IO        = 0x1,
    BAR_IO_MASK   = 0xFFFFFFFCU,
    BAR_MEM_MASK  = 0xFFFFFFF0U,

    CAP_MSI       = 0x05U,
    CAP_MSIX      = 0x11U,
    CAP_PCIE      = 0x10U,
  };


  unsigned conf_read(unsigned bdf, unsigned dword)
  {
    MessageHwPciConfig msg(bdf, dword);
    _bus_pcicfg.send(msg, true);
    return msg.value;
  }


  void conf_write(unsigned bdf, unsigned dword, unsigned value)
  {
    MessageHwPciConfig msg(bdf, dword, value);
    _bus_pcicfg.send(msg, true);
  }


  /**
   * Induce the number of the bars from the header-type.
   */
  unsigned count_bars(unsigned bdf) {

    switch((conf_read(bdf, 0x3) >> 24) & 0x7f) {
    case 0: return 6;
    case 1: return 2;
    default: return 0;
    }
  }


  /**
   * Searches for a given device and returns the bdf of it.
   */
  unsigned search_device(unsigned theclass, unsigned subclass, unsigned instance)
  {
    for (unsigned bus = 0; bus < 256; bus++)
      {
	// skip empty busses
	//if (conf_read(bus << 8, 0) == ~0u) continue;
	for (unsigned dev=0; dev < 32; dev++)
	  {
	    unsigned char maxfunc = 1;
	    for (unsigned func=0; func < maxfunc; func++)
	      {
		unsigned bdf =  (bus << 8) | (dev << 3) | func;
		unsigned value = conf_read(bdf, 2);
		if (value == ~0UL) continue;
		if (maxfunc == 1 && conf_read(bdf, 3) & 0x800000)
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
   * Scan the PCI root bus for bridges.
   */
  unsigned search_bridge(unsigned dst) {

    unsigned char dstbus = dst >> 8;

    for (unsigned dev = 0; dev < 32; dev++) {

      unsigned char maxfunc = 1;
      for (unsigned func = 0; func < maxfunc; func++) {

	unsigned bdf =  (dev << 3) | func;
	unsigned value = conf_read(bdf, 2);
	if (value == ~0UL) continue;

	unsigned char header = conf_read(bdf, 3) >> 16;
	if (maxfunc == 1 && header & 0x80)  maxfunc = 8;
	if ((header & 0x7f) != 1) continue;

	// we have a bridge
	unsigned b = conf_read(bdf, 6);
	if ((((b >> 8) & 0xff) <= dstbus) && (((b >> 16) & 0xff) >= dstbus))
	  return bdf;
      }
    }
    return 0;
  }


  /**
   * Program the nr-th MSI/MSI-X vector of the given device.
   */
  unsigned get_gsi_msi(DBus<MessageHostOp> &bus_hostop, unsigned bdf, unsigned nr, void *msix_table = 0)
  {
    unsigned msix_offset = find_cap(bdf, CAP_MSIX);
    unsigned msi_offset = find_cap(bdf, CAP_MSI);
    if (!(msix_offset || msi_offset)) Logging::panic("No MSI support in %x for %x", bdf, nr);

    MessageHostOp msg1 = msg1.attach_msi(~0UL, true, bdf, "gsi msi");
    if (!bus_hostop.send(msg1)) Logging::panic("could not attach to msi for bdf %x\n", bdf);
    if (!msg1.msi_address)  Logging::printf("Attach to MSI %x failed for bdf %x with (%llx,%x) - IRQs may be broken!\n", nr, bdf, msg1.msi_address, msg1.msi_value);

    // MSI-X
    if (msix_offset) {
      if (!msix_table) {
	unsigned table_offset = conf_read(bdf, msix_offset + 1);
	unsigned long long base = bar_base(bdf, BAR0 + (table_offset & 0x7)) + (table_offset & ~0x7u);

	// Map the MSI-X bar
	MessageHostOp msg2(MessageHostOp::OP_ALLOC_IOMEM, base & (~0xffful), 0x1000);
	if (!bus_hostop.send(msg2) || !msg2.ptr)
	  Logging::panic("can not map MSIX bar %lx+%lx", msg2.value, msg2.len);

	msix_table = msg2.ptr + (base & 0xfff);
      }

      volatile unsigned *_msix_table = reinterpret_cast<volatile unsigned *>(msix_table);
      _msix_table[nr*4 + 0]  = msg1.msi_address;
      _msix_table[nr*4 + 1]  = msg1.msi_address >> 32;
      _msix_table[nr*4 + 2]  = msg1.msi_value;
      _msix_table[nr*4 + 3] &= ~1;
      conf_write(bdf, msix_offset, 1U << 31);
    } else if (msi_offset) {
      unsigned ctrl = conf_read(bdf, msi_offset);
      unsigned base = msi_offset + 1;
      conf_write(bdf, base+0, msg1.msi_address);
      conf_write(bdf, base+1, msg1.msi_address >> 32);
      if (ctrl & 0x800000) base += 1;
      conf_write(bdf, base+1, msg1.msi_value);

      // we use only a single message and enable MSIs here
      conf_write(bdf, msi_offset, (ctrl & ~0x700000) | 0x10000);
      Logging::printf("MSI %x enabled for bdf %x MSI %llx/%x\n", msg1.msi_gsi, bdf, msg1.msi_address, msg1.msi_value);
    }

    return msg1.msi_gsi;
  }


  /**
   * Returns the gsi and enables them.
   */
  unsigned get_gsi(DBus<MessageHostOp> &bus_hostop, DBus<MessageAcpi> &bus_acpi, unsigned bdf, unsigned nr,
		   bool level=false, void *msix_table = 0)
  {
    // If the device is MSI or MSI-X capable, don't use legacy
    // interrupts.
    if (find_cap(bdf, CAP_MSIX) || find_cap(bdf, CAP_MSI))
      return get_gsi_msi(bus_hostop, bdf, nr, msix_table);

    if (nr != 0)
      Logging::printf("XXX Trying to program vector %d, but we only have legacy interrupts!\n", nr);

    // normal GSIs -  ask atare
    unsigned char pin = conf_read(bdf, 0xf) >> 8;
    if (!pin) { Logging::printf("No IRQ PINs connected on %x\n", bdf ); return ~0u; }
    MessageAcpi msg3(search_bridge(bdf), bdf, pin - 1);
    if (!bus_acpi.send(msg3, true)) {
      msg3.gsi = conf_read(bdf, 0xf) & 0xff;
      Logging::printf("No clue which GSI %x_%x is triggering - fall back to PIC irq 0x%x\n", bdf, pin, msg3.gsi);
    }

    // attach to the IRQ
    MessageHostOp msg = MessageHostOp::attach_irq(msg3.gsi | (level ? 0x100 : 0), ~0U, true, "pci");
    if (!bus_hostop.send(msg)) return ~0ul;
    return msg3.gsi;
  }


  /**
   * Find the position of a legacy PCI capability.
   */
  unsigned find_cap(unsigned bdf, unsigned char id)
  {
    if ((conf_read(bdf, 1) >> 16) & 0x10 /* Capabilities supported? */)
      for (unsigned char offset = conf_read(bdf, 0xd);
	   (offset != 0) && !(offset & 0x3);
	   offset = conf_read(bdf, offset >> 2) >> 8)
	if ((conf_read(bdf, offset >> 2) & 0xFF) == id)
	  return offset >> 2;
    return 0;
  }


  /**
   * Find the position of an extended PCI capability.
   */
  unsigned find_extended_cap(unsigned bdf, unsigned short id)
  {
    unsigned header, offset;

    if ((find_cap(bdf, CAP_PCIE)) && (~0UL != conf_read(bdf, 0x40)))
      for (offset = 0x100, header = conf_read(bdf, offset >> 2);
	   offset != 0;
	   offset = header >> 20, header = conf_read(bdf, offset >> 2))
	if ((header & 0xFFFF) == id)
	  return offset >> 2;
    return 0;
  }


  /**
   * Get the base and the type of a bar.
   */
  unsigned long long bar_base(unsigned bdf, unsigned bar, unsigned *type = 0)
  {
    unsigned val = conf_read(bdf, bar);
    if ((val & BAR_IO) == BAR_IO) {
      /* XXX */
      if (type) *type = BAR_IO;
      return val & ~0x3;
    } else {
      if (type) *type = val & BAR_TYPE_MASK;
      switch (val & BAR_TYPE_MASK) {
      case BAR_TYPE_32B: return val & BAR_MEM_MASK;
      case BAR_TYPE_64B: return (static_cast<unsigned long long>(conf_read(bdf, bar + 1))<<32) | (val & BAR_MEM_MASK);
      default: return ~0ULL;
      };
    }
  }


  /**
   * Determines BAR size. You should probably disable interrupt
   * delivery from this device, while querying BAR sizes.
   */
  unsigned long long bar_size(unsigned bdf, unsigned bar, bool *is64bit = 0)
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
	unsigned old_hi = conf_read(bdf, bar + 1);
	conf_write(bdf, bar, 0xFFFFFFFFU);
	conf_write(bdf, bar + 1, 0xFFFFFFFFU);
	unsigned long long bar_size = static_cast<unsigned long long>(conf_read(bdf, bar + 1))<<32;
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

 HostPci(DBus<MessageHwPciConfig> &bus_pcicfg, DBus<MessageHostOp> &bus_hostop) : _bus_pcicfg(bus_pcicfg), _bus_hostop(bus_hostop) {};
};
