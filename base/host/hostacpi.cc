/** @file
 * HostAcpi driver.
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

#include "nul/motherboard.h"

/**
 * Provide access to the ACPI tables.
 *
 * Features: RSDT, table search, DSDT
 * Missing:  XSDT, DSDT_X
 */
class HostAcpi : public StaticReceiver<HostAcpi>
{

  DBus<MessageHostOp> &_bus_hostop;

  char *map_self(unsigned long address, unsigned size)
  {
    MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, address, size);
    if (!_bus_hostop.send(msg) || !msg.ptr)
      {
        Logging::printf("ac: %s failed to allocate iomem %lx+%x\n", __PRETTY_FUNCTION__, address, size);
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


  bool check_table(unsigned address, MessageAcpi &msg, const char *name) {
    char * t = map_self(address, 0x1000);
    //Logging::printf("ac: acpi table at %x %p sig %4s\n", address, t, reinterpret_cast<GenericAcpiTable *>(t)->signature);
    if (!memcmp(reinterpret_cast<GenericAcpiTable *>(t)->signature, name, 4))
      {
        unsigned size = reinterpret_cast<GenericAcpiTable *>(t)->size;
        if (size > 0x1000)  t = map_self(address, size);
        if (acpi_checksum(t, size)) return false;
        msg.table = t;
        msg.len = size;
        return true;
      }
    return false;
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
        unsigned instance = msg.instance;
        //Logging::printf("ac: search ACPI table %s\n", msg.name);
        char *table = acpi_get_rsdp();
        if (!table) break;

        bool search_for_dsdt = !memcmp(msg.name, "DSDT", 4);
        const char *name = search_for_dsdt ? "FACP" : msg.name;

        // get rsdt
        table = map_self(*reinterpret_cast<unsigned *>(table + 0x10), 0x1000);
        if (!table) break;
        unsigned rsdt_size = reinterpret_cast<GenericAcpiTable *>(table)->size;
        if (rsdt_size > 0x1000) table = map_self(*reinterpret_cast<unsigned *>(table + 0x10), rsdt_size);
        if (!table || !rsdt_size) break;
        if (acpi_checksum(table, rsdt_size)) { Logging::printf("ac: RSDT checksum invalid\n"); break; }

        // iterate over rsdt_entries
        unsigned *rsdt_entries = reinterpret_cast<unsigned *>(table + sizeof(GenericAcpiTable));
        for (unsigned i=0; i < ((rsdt_size - sizeof(GenericAcpiTable)) / 4); i++)
          if (check_table(rsdt_entries[i], msg, name) && !instance--) {
            if (search_for_dsdt)
              return check_table(*reinterpret_cast<unsigned *>(msg.table + 40), msg, "DSDT");
            return true;
          }
      }
      case MessageAcpi::ACPI_GET_IRQ:
        break;
    }
    return false;
  }

  HostAcpi(DBus<MessageHostOp> &bus_hostop) : _bus_hostop(bus_hostop) {};
};


PARAM_HANDLER(hostacpi,
	      "hostacpi - provide ACPI tables to drivers.")
{
  HostAcpi *dev = new HostAcpi(mb.bus_hostop);
  mb.bus_acpi.add(dev, HostAcpi::receive_static<MessageAcpi>);
}
