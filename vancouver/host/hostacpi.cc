/**
 * HostAcpi driver.
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

#include "vmm/motherboard.h"

/**
 * Provide access to the ACPI tables.
 *
 * Features: RSDT, table search
 * Missing:  XSDT, GSIs per PCI device
 */
class HostAcpi : public StaticReceiver<HostAcpi>
{

  const char *debug_getname() {  return "HostACPI"; }
  DBus<MessageHostOp> &_bus_hostop;

  char *map_self(unsigned long address, unsigned size)
  {
    MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, address, size);
    if (!_bus_hostop.send(msg) || !msg.ptr)
      {
	Logging::printf("%s failed to allocate iomem %lx+%x\n", __PRETTY_FUNCTION__, address, size);
	return 0;
      }
    return msg.ptr;
  }


  /**
   * Calculate the ACPI checksum of a table.
   */
  char  acpi_checksum(char *table, unsigned count)
  {
    char res = 0;
    while (count--)
      res += table[count];
    return res;
  }

  /**
   * Return the rsdp.
   */
  char *acpi_get_rsdp()
  {
    // search in BIOS readonly memory range
    char *area = map_self(0xe0000, 0x100000 - 0xe0000);
    for (long addr = 0; area && addr < 0x100000 - 0xe0000; addr += 16)
      {
	if (!memcmp(area + addr, "RSD PTR ", 8) && !acpi_checksum(area + addr, 20))
	  return area+addr;
      }

    // search in ebda
    area = map_self(0x0, 1<<12);
    if (area)  area = map_self((*reinterpret_cast<unsigned short *>(area + 0x40e)) << 4, 1024);
    for (long addr = 0; area && addr < 1024; addr += 16)
      {
	if (!memcmp(area + addr, "RSD PTR ", 8) && !acpi_checksum(area + addr, 20))
	  return area+addr;
      }
    return 0;
  }

  struct GenericAcpiTable
  {
    char signature[4];
    unsigned size;
    char rev;
    char checksum;
    char oemid[6];
    char oemtabid[8];
    unsigned oemrev;
    char creator[4];
    unsigned crev;
  };

public:
  bool  receive(MessageAcpi &msg)
  {
    switch (msg.type)
      {
      case MessageAcpi::ACPI_GET_TABLE:
	{
	  Logging::printf("search ACPI table %s\n", msg.name);
	  char *table = acpi_get_rsdp();
	  if (!table) break;

	  // get rsdt
	  table = map_self(*reinterpret_cast<unsigned *>(table + 0x10), 0x1000);
	  if (!table) break;
	  unsigned rsdt_size = reinterpret_cast<GenericAcpiTable *>(table)->size;
	  if (rsdt_size > 0x1000) table = map_self(*reinterpret_cast<unsigned *>(table + 0x10), rsdt_size);
	  if (!table || !rsdt_size) break;
	  if (acpi_checksum(table, rsdt_size)) { Logging::printf("RSDT checksum invalid\n"); break; }

	  // iterate over rsdt_entries
	  unsigned *rsdt_entries = reinterpret_cast<unsigned *>(table + sizeof(GenericAcpiTable));
	  for (unsigned i=0; i < ((rsdt_size - sizeof(GenericAcpiTable)) / 4); i++)
	    {
	      char * t = map_self(rsdt_entries[i], 0x1000);
	      Logging::printf("acpi table[%x] at %x %p sig %4s\n", i, rsdt_entries[i], t, reinterpret_cast<GenericAcpiTable *>(t)->signature);
	      if (!memcmp(reinterpret_cast<GenericAcpiTable *>(t)->signature, msg.name, 4))
		{
		  unsigned size = reinterpret_cast<GenericAcpiTable *>(t)->size;
		  if (size > 0x1000)  t = map_self(rsdt_entries[i], size);
		  if (acpi_checksum(t, size)) continue;
		  msg.table = t;
		  return true;
		}
	    }
	}
      default:
	Logging::printf("unimplemented op %x\n", msg.type);
	break;
      }
    return false;
  }

  HostAcpi(DBus<MessageHostOp> &bus_hostop) : _bus_hostop(bus_hostop) {};
};


PARAM(hostacpi,
      {
	Device *dev = new HostAcpi(mb.bus_hostop);
	mb.bus_acpi.add(dev, HostAcpi::receive_static<MessageAcpi>);
      },
      "hostacpi - provide ACPI tables to drivers.")
