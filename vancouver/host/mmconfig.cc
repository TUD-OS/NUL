/**
 * PCI config space access via mmconfig.
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

#include "vmm/motherboard.h"
#include <cstdint>

struct AcpiMCFG {
  uint32_t magic;		// MCFG
  uint32_t len;
  uint8_t rev;
  uint8_t checksum;
  char oem_id[6];
  char model_id[8];
  uint32_t oem_rev;
  uint32_t creator_vendor;
  uint32_t creator_utility;
  char _res[8];

  struct Entry {
    uint64_t base;
    uint16_t pci_seg;
    uint8_t pci_bus_start;
    uint8_t pci_bus_end;
    uint32_t _res;
  } __attribute__((packed)) entries[];

} __attribute__((packed));

class PciMMConfigAccess : public StaticReceiver<PciMMConfigAccess>
{
  const char *debug_getname() { return "PciMMConfigAccess"; };

  uint8_t _start_bus;
  uint8_t _end_bus;

  char *_mmconfig;

public:

  bool receive(MessageExtPciCfg &msg) {
    uint8_t bus = msg.bdf >> 8;
    if ((bus >= _start_bus) && (bus <= _end_bus)) {
      uint32_t *field = (uint32_t *)((msg.bdf<<12) + msg.reg + _mmconfig);

      switch (msg.type) {
      case MessageExtPciCfg::TYPE_READ:  msg.value = *field; break;
      case MessageExtPciCfg::TYPE_WRITE: *field = msg.value; break;
      }

      return true;
    } else {
      return false;
    }
  }

  
  PciMMConfigAccess(uint8_t start_bus, uint8_t end_bus, char *mmconfig) {

  }
};

PARAM(mmconfig,
      {
	MessageAcpi msg("MCFG");
	if (!mb.bus_acpi.send(msg) || !msg.table) {
	  Logging::printf("XXX No MCFG table found.\n");
	} else {
	  AcpiMCFG *mcfg = reinterpret_cast<AcpiMCFG *>(msg.table);
	  // XXX Compute checksum;
	  void *mcfg_end = reinterpret_cast<char *>(mcfg) + mcfg->len;

	  for (AcpiMCFG::Entry *entry = mcfg->entries; entry < mcfg_end; entry++) {
	    Logging::printf("mmconfig: base 0x%llx seg %02x bus %02x-%02x\n",
			    entry->base, entry->pci_seg,
			    entry->pci_bus_start, entry->pci_bus_end);

	    if (entry->pci_seg != 0) {
	      Logging::printf("mmconfig: Skip. We do not support multiple PCI segments.\n");
	      continue;
	    }

	    uint8_t buses = entry->pci_bus_end - entry->pci_bus_start + 1;
	    size_t size = 8*32*4096*buses;
	    MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, entry->base, size);

	    if (!mb.bus_hostop.send(msg) || !msg.ptr) {
	      Logging::printf("%s failed to allocate iomem %llx+%x\n", __PRETTY_FUNCTION__, entry->base, size);
	      return;
	    }

	    Device *dev = new PciMMConfigAccess(entry->pci_bus_start, entry->pci_bus_end, msg.ptr);
	    mb.bus_hwextpcicfg.add(dev, &PciMMConfigAccess::receive_static<MessageExtPciCfg>);
	  }
	}
      },
      "mmconfig - provide HW PCI config space access via mmconfig.");
