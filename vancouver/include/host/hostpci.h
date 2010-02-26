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

#include "nul/motherboard.h"

/**
 * A helper for PCI config space access.
 */
class HostPci
{
  DBus<MessagePciConfig> &_bus_pcicfg;
  DBus<MessageHostOp>    &_bus_hostop;

 public:

  enum {
    BAR0          = 0x10,
    MAX_BAR       = 6,

    BAR_TYPE_MASK = 0x6,
    BAR_TYPE_32B  = 0x0,
    BAR_TYPE_64B  = 0x4,
    BAR_IO        = 0x1,
    BAR_IO_MASK   = 0xFFFFFFFCU,
    BAR_MEM_MASK  = 0xFFFFFFF0U,

    CAP_MSI       = 0x05U,
    CAP_MSIX      = 0x11U,
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
    if ((val & BAR_IO) == BAR_IO)  return val & BAR_IO_MASK;

    switch (val & BAR_TYPE_MASK) {
    case BAR_TYPE_32B: return val & BAR_MEM_MASK;
    case BAR_TYPE_64B: return (static_cast<unsigned long long>(conf_read(bdf, bar + 4))<<32) | (val & BAR_MEM_MASK);
    default: return ~0ULL;
    };
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
	unsigned old_hi = conf_read(bdf, bar + 4);
	conf_write(bdf, bar, 0xFFFFFFFFU);
	conf_write(bdf, bar + 4, 0xFFFFFFFFU);
	unsigned long long bar_size = ((unsigned long long)conf_read(bdf, bar + 4))<<32;
	bar_size = (((bar_size | conf_read(bdf, bar)) & ~0xFULL) ^ ~0ULL) + 1;
	size = bar_size;
	conf_write(bdf, bar + 4, old_hi);
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
   * Scan the PCI root bus for bridges.
   */
  unsigned search_bridge(unsigned dst) {

    unsigned char dstbus = dst >> 8;

    for (unsigned dev = 0; dev < 32; dev++) {

      unsigned char maxfunc = 1;
      for (unsigned func = 0; func < maxfunc; func++) {

	unsigned bdf =  (dev << 3) | func;
	unsigned value = conf_read(bdf, 0x8);
	if (value == ~0UL) continue;

	unsigned char header = conf_read(bdf, 0xc) >> 16;
	if (maxfunc == 1 && header & 0x80)  maxfunc = 8;
	if ((header & 0x7f) != 1) continue;

	// we have a bridge
	unsigned b = conf_read(bdf, 0x18);
	if ((((b >> 8) & 0xff) <= dstbus) && (((b >> 16) & 0xff) >= dstbus))
	  return bdf;
      }
    }
    return 0;
  }


  /**
   * Returns the gsi and enables them.
   */
  unsigned get_gsi(DBus<MessageHostOp> &bus_hostop, DBus<MessageAcpi> &bus_acpi, unsigned bdf, unsigned nr, unsigned long gsi=~0ul, bool level=false) {

    unsigned msix_offset = find_cap(bdf, CAP_MSIX);
    unsigned msi_offset = find_cap(bdf, CAP_MSI);

    // XXX global disable MSI+MSI-X switch
    //msi_offset = 0;

    // attach to an MSI
    MessageHostOp msg1(MessageHostOp::OP_ATTACH_MSI, bdf);
    if ((msi_offset || msix_offset) && !bus_hostop.send(msg1))
      Logging::panic("could not attach to msi for bdf %x\n", bdf);

    // MSI-X
    if (msix_offset)
      {
	unsigned ctrl1 = conf_read(bdf, msix_offset + 0x4);
	unsigned long base = bar_base(bdf, (ctrl1 & 0x7)*4 + BAR0) + (ctrl1 & ~0x7u);

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
	return msg1.msi_gsi;
      }

    // MSI
    if (msi_offset)
      {
	unsigned ctrl = conf_read(bdf, msi_offset);
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


    // normal GSIs -  ask atare
    unsigned char pin = conf_read(bdf, 0x3c) >> 8;
    if (!pin) { Logging::printf("No IRQ PINs connected on %x\n", bdf ); return ~0u; }
    MessageAcpi msg3(search_bridge(bdf), bdf, pin - 1);
    if (gsi == ~0UL && bus_acpi.send(msg3))
      gsi = msg3.gsi;
    else
      Logging::panic("No glue which GSI %x_%x is triggering\n", bdf, pin);


    // attach to the IRQ
    MessageHostOp msg(MessageHostOp::OP_ATTACH_IRQ, gsi | (level ? 0x100 : 0));
    if (!bus_hostop.send(msg)) return ~0ul;
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

 HostPci(DBus<MessagePciConfig> &bus_pcicfg, DBus<MessageHostOp> &bus_hostop) : _bus_pcicfg(bus_pcicfg), _bus_hostop(bus_hostop) {};
};
