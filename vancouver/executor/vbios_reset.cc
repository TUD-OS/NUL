/** @file
 * Virtual Bios reset routines.
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
#include "executor/bios.h"

bool use_x2apic_mode;
PARAM_HANDLER(x2apic_mode,
	      "x2apic_mode - enable x2apic mode in the LAPICs")
{use_x2apic_mode = true;}

/**
 * Virtual Bios reset routines.
 * Features: init of PIC, PIT, bda+ebda, ACPI tables
 * Missing: flexible ACPI table size
 */
class VirtualBiosReset : public StaticReceiver<VirtualBiosReset>, public BiosCommon
{
  enum {
    MAX_RESOURCES = 32,
    SIZE_EBDA_KB  = 5,
    /**
     * EBDA Layout
     * 0x0000 - 0x0200 compatible EBDA
     * 0x0200 - 0x0220 rsdp
     * 0x1000 - 0x2000 8x16 font
     */
  };
#define ACPI_OEM_ID        " NOVA "
#define ACPI_MANUFACTURER "bk@vmmon"

  char     *_mem_ptr;
  unsigned  _mem_size;

  struct Resource {
    const char *name;
    unsigned offset;
    unsigned length;
    bool     acpi_table;
    Resource() {}
    Resource(const char *_name, unsigned _offset, unsigned _length, bool _acpi_table) : name(_name), offset(_offset), length(_length), acpi_table(_acpi_table)  {}
  } _resources[MAX_RESOURCES];




  /**
   * called on reset.
   */
  bool reset_helper(MessageBios &msg)
  {
    CpuState *state = msg.cpu;
    VCpu *vcpu = msg.vcpu;

    bool bsp = !vcpu->get_last();

    // the APIC
    state->eax = 0xfee00800 | (bsp ? 0x100U : 0U);
    state->edx = 0;
    state->ecx = 0x1b;
    CpuMessage msg1(CpuMessage::TYPE_WRMSR, state, MTD_GPR_ACDB);
    vcpu->executor.send(msg1, true);


    // enable SVR, LINT0, LINT1
    unsigned m[] = { 0x1ff, bsp ? 0x700U : 0x10700U, 0x400};
    MessageMem msg2[] = {
      MessageMem(false, 0xfee000f0, m+0),
      MessageMem(false, 0xfee00350, m+1),
      MessageMem(false, 0xfee00360, m+2),
    };
    for (unsigned j=0; j < sizeof(msg2) / sizeof(*msg2); j++)  vcpu->mem.send(msg2[j], true);


    // switch to x2apic mode?
    if (use_x2apic_mode) {
      state->eax = 0xfee00c00 | (bsp ? 0x100 : 0);
      vcpu->executor.send(msg1, true);
    }


    if (!bsp) return jmp_hlt(msg);

    // we are a BSP and init the platform

    // initialize PIT0
    // let counter0 count with minimal freq of 18.2hz
    outb(0x40+3, 0x24);
    outb(0x40+0, 0);

    // let counter1 generate 15usec refresh cycles
    outb(0x40+3, 0x56);
    outb(0x40+1, 0x12);

    // the master PIC
    // ICW1-4+IMR
    outb(0x20+0, 0x11);
    outb(0x20+1, 0x08); // offset 0x08
    outb(0x20+1, 0x04); // has slave on 2
    outb(0x20+1, 0x0f); // is buffer, master, AEOI and x86
    outb(0x20+1, 0xfc); // TIMER+keyboard IRQ needed

    // the slave PIC + IMR
    outb(0xa0+0, 0x11);
    outb(0xa0+1, 0x70); // offset 0x70
    outb(0xa0+1, 0x02); // is slave on 2
    outb(0xa0+1, 0x0b); // is buffer, slave, AEOI and x86
    outb(0xa0+1, 0xff);



    // INIT resources
    memset(_resources, 0, sizeof(_resources));

    MessageMemRegion msg3(0);
    check1(false, !_mb.bus_memregion.send(msg3) || !msg3.ptr || !msg3.count, "no low memory available");

    // were we start to allocate stuff
    _mem_ptr = msg3.ptr;
    _mem_size = msg3.count << 12;

    // we use the lower 640k of memory
    if (_mem_size > 0xa0000) _mem_size = 0xa0000;

    // trigger discovery
    MessageDiscovery msg4;
    _mb.bus_discovery.send_fifo(msg4);

    // the ACPI IRQ is 9
    discovery_write_dw("FACP",  46,          9, 2);

    // store what remains on memory in KB
    discovery_write_dw("bda", 0x13, _mem_size >> 10, 2);
    return jmp_int(msg, 0x19);
  }



  unsigned alloc(unsigned size, unsigned alignment) {
    if ((size + alignment + 0x1000) > _mem_size) return 0;
    _mem_size -= size;
    _mem_size &= ~alignment;

    // clear region
    memset(_mem_ptr + _mem_size, 0, size);
    return _mem_size;
  }


  Resource * get_resource(const char *name) {
    for (unsigned i = 0; i < MAX_RESOURCES; i++) {
      check1(0, !_resources[i].name && !create_resource(i, name), "could not create resource");
      if (!strcmp(_resources[i].name, name)) return _resources + i;
    }
    return 0;
  }


  unsigned acpi_tablesize(Resource *r) { return *reinterpret_cast<unsigned *>(_mem_ptr + r->offset + 4); }


  void fix_acpi_checksum(Resource *r, unsigned length, unsigned chksum_offset = 9) {
    assert(r);
    char value = 0;
    for (unsigned i=0; i < length && i < r->length; i++)
      value += _mem_ptr[r->offset + i];
    _mem_ptr[r->offset + chksum_offset] -= value;
  }


  void init_acpi_table(const char *name) {
    discovery_write_st(name,  0, name, 4);
    discovery_write_dw(name,  8, 1, 1);
    discovery_write_st(name, 10, ACPI_OEM_ID, 6);
    discovery_write_st(name, 16, ACPI_MANUFACTURER, 8);
    discovery_write_dw(name, 24, 1, 4);
    discovery_write_dw(name, 28, 0, 4);
    discovery_write_dw(name, 32, 0, 4);
  }


  bool create_resource(unsigned index, const char *name) {
    if (!strcmp("realmode idt", name)) {
      _resources[index] = Resource(name, 0, 0x400, false);
      memset(_mem_ptr + _resources[index].offset, 0, _resources[index].length);
    }
    else if (!strcmp("bda", name)) {
      _resources[index] = Resource(name, 0x400, 0x200, false);
      memset(_mem_ptr + _resources[index].offset, 0, _resources[index].length);
    }
    else if (!strcmp("ebda", name)) {
      unsigned ebda;
      check1(false, !(ebda = alloc(SIZE_EBDA_KB << 10, 0x10)));
      _resources[index] = Resource(name, ebda, SIZE_EBDA_KB << 10, false);
      discovery_write_dw("bda", 0xe, ebda >> 4, 2);
      discovery_write_dw(name, 0, SIZE_EBDA_KB, 1);
    }
    else if (!strcmp("RSDP", name)) {
      Resource *r;
      check1(false, !(r = get_resource("ebda")));
      _resources[index] = Resource(name, r->offset + 0x200, 36, false);
      discovery_write_st(name, 0,  "RSD PTR ", 8);
      discovery_write_st(name, 9,  ACPI_OEM_ID, 6);
      // revision = 0 => ACPI version 1.0
      discovery_write_dw(name, 15, 0, 1);
      fix_acpi_checksum(_resources + index, 20, 8);
    }
    else {
      // we create an ACPI table
      unsigned table;
      check1(false, !(table = alloc(0x1000, 0x10)), "allocate ACPI table failed");
      _resources[index] = Resource(name, table, 0x1000, true);
      init_acpi_table(name);
      if (!strcmp(name, "RSDT")) {
        discovery_write_dw("RSDP", 16, table, 4);

        Resource *r;
        check1(false, !(r = get_resource("RSDP")));
        fix_acpi_checksum(r, 20, 8);
      }
      else {
        // add them to the RSDT
        Resource *rsdt;
        check1(false, !(rsdt = get_resource("RSDT")));
        unsigned rsdt_length = acpi_tablesize(rsdt);

        // and write the pointer to the RSDT
        discovery_write_dw("RSDT", rsdt_length, table, 4);
      }
    }
    return true;
  }

public:

  bool  receive(MessageBios &msg) {
    switch(msg.irq) {
    case RESET_VECTOR:
      return reset_helper(msg);
    case 0x18:
      Logging::printf("INT18 - new try\n");
      return jmp_int(msg, 0x19);
    default:    return false;
    }
  }


  /**
   * React on device discovery.
   */
  bool  receive(MessageDiscovery &msg) {
    switch (msg.type) {
    case MessageDiscovery::WRITE:
      {
        Resource *r;
        unsigned needed_len = msg.offset + msg.count;
        check1(false, !(r = get_resource(msg.resource)));
        check1(false, needed_len > r->length, "WRITE no idea how to increase the table %s size from %d to %d", msg.resource, r->length, needed_len);

        unsigned table_len = acpi_tablesize(r);
        // increase the length of an ACPI table.
        if (r->acpi_table && msg.offset >= 8 && needed_len > table_len) {
          discovery_write_dw(r->name, 4, needed_len, 4);
          table_len = needed_len;
        }
        memcpy(_mem_ptr + r->offset + msg.offset, msg.data, msg.count);

        // and fix the checksum
        if (r->acpi_table)   fix_acpi_checksum(r, table_len);
      }
      break;
    case MessageDiscovery::READ:
      {
        Resource *r;
        unsigned needed_len = msg.offset + 4;
        check1(false, !(r = get_resource(msg.resource)));
        check1(false, needed_len > r->length, "READ no idea how to increase the table %s size from %d to %d", msg.resource, r->length, needed_len);
        memcpy(msg.dw, _mem_ptr + r->offset + msg.offset, 4);
      }
      break;
    case MessageDiscovery::DISCOVERY:
    default:
      return false;
    }
    return true;
  }


  VirtualBiosReset(Motherboard &mb) : BiosCommon(mb) {}
};

PARAM_HANDLER(vbios_reset,
	      "vbios_reset - provide reset handling for virtual BIOS functions.")
{
  VirtualBiosReset * dev = new VirtualBiosReset(mb);
  mb.bus_bios.add(dev,      VirtualBiosReset::receive_static<MessageBios>);
  mb.bus_discovery.add(dev, VirtualBiosReset::receive_static<MessageDiscovery>);
}
