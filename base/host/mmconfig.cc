/** @file
 * PCI config space access via mmconfig.
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include "nul/motherboard.h"

struct AcpiMCFG {
  unsigned magic;
  unsigned len;
  unsigned char rev;
  unsigned char checksum;
  char oem_id[6];
  char model_id[8];
  unsigned oem_rev;
  unsigned creator_vendor;
  unsigned creator_utility;
  char _res[8];

  struct Entry {
    unsigned long long base;
    unsigned short pci_seg;
    unsigned char pci_bus_start;
    unsigned char pci_bus_end;
    unsigned _res;
  } __attribute__((packed)) entries[];

} __attribute__((packed));


class PciMMConfigAccess : public StaticReceiver<PciMMConfigAccess>
{
  unsigned  _start_bdf;
  unsigned  _bdf_size;
  unsigned *_mmconfig;

public:

  bool receive(MessageHwPciConfig &msg) {
    if (!in_range(msg.bdf, _start_bdf, _bdf_size) || msg.dword >= 0x400)  return false;

    unsigned *field = _mmconfig + (msg.bdf << 10) + msg.dword;
    switch (msg.type) {
    case MessagePciConfig::TYPE_READ:  msg.value = *field; break;
    case MessagePciConfig::TYPE_WRITE: *field = msg.value; break;
    case MessagePciConfig::TYPE_PTR:   msg.ptr = field;    break;
    default: return false;
    }
    return true;
  }


  PciMMConfigAccess(unsigned start_bdf, unsigned bdf_size, unsigned *mmconfig)
  : _start_bdf(start_bdf), _bdf_size(bdf_size), _mmconfig(mmconfig) {}
};

PARAM_HANDLER(mmconfig,
	      "mmconfig - provide HW PCI config space access via mmconfig.")
{
  MessageAcpi msg("MCFG");
  check0(!mb.bus_acpi.send(msg, true) || !msg.table, "mm: XXX No MCFG table found.");

  AcpiMCFG *mcfg = reinterpret_cast<AcpiMCFG *>(msg.table);

  for (unsigned i = 0; i < (mcfg->len - sizeof(AcpiMCFG)) / sizeof(AcpiMCFG::Entry); i++) {
    AcpiMCFG::Entry *entry = mcfg->entries + i;
    Logging::printf("mm: mmconfig: base 0x%llx seg %02x bus %02x-%02x\n",
		    entry->base, entry->pci_seg,
		    entry->pci_bus_start, entry->pci_bus_end);

    unsigned buses = entry->pci_bus_end - entry->pci_bus_start + 1;
    unsigned long size = buses * 32 * 8 * 4096;
    MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, entry->base, size);

    if (!mb.bus_hostop.send(msg) || !msg.ptr) {
      Logging::printf("mm: %s failed to allocate iomem %llx+%lx\n", __PRETTY_FUNCTION__, entry->base, size);
      return;
    }

    PciMMConfigAccess *dev = new PciMMConfigAccess((entry->pci_seg << 16) + entry->pci_bus_start * 32 * 8, buses * 32 * 8, reinterpret_cast<unsigned *>(msg.ptr));
    mb.bus_hwpcicfg.add(dev, PciMMConfigAccess::receive_static<MessageHwPciConfig>);
  }
}

