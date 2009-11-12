/**
 * Multiboot executor.
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
#include "sys/elf.h"


/**
 * Provide Multiboot ability.
 *
 * State: unstable
 * Features: CPU init, elf-decoding, MBI creation, memory-map
 * Missing: variable trigger address, request modules from sigma0
 */
class MultibootExecutor : public StaticReceiver<MultibootExecutor>
{
public:
  enum mbi_enum
    {
      MBI_MAGIC                  = 0x2badb002,
      MBI_FLAG_MEM               = 1 << 0,
      MBI_FLAG_CMDLINE           = 1 << 2,
      MBI_FLAG_MODS              = 1 << 3,
      MBI_FLAG_MMAP              = 1 << 6,
      MBI_FLAG_BOOT_LOADER_NAME  = 1 << 9,
      MBI_FLAG_VBE               = 1 << 11,
    };


  struct Mbi
  {
    unsigned flags;
    unsigned mem_lower;
    unsigned mem_upper;
    unsigned dummy1;
    unsigned cmdline;
    unsigned mods_count;
    unsigned mods_addr;
    unsigned dummy2[4];
    unsigned mmap_length;
    unsigned mmap_addr;
    unsigned dummy3[3];
    unsigned boot_loader_name;
    unsigned dummy4;
    unsigned vbe_control_info;
    unsigned vbe_mode_info;
    unsigned short vbe_mode;
    unsigned short vbe_interface_seg;
    unsigned short vbe_interface_off;
    unsigned short vbe_interface_len;
  };


  struct Module
  {
    unsigned mod_start;
    unsigned mod_end;
    unsigned string;
    unsigned reserved;
  };

  struct MbiMmap
  {
    unsigned size;
    unsigned long long base __attribute__((packed));
    unsigned long long length  __attribute__((packed));
    unsigned type;
  };
private:
  Motherboard &_mb;
  unsigned long _base;
  const char *debug_getname() { return "MultibootExecutor"; };
  void debug_dump() {  
    Device::debug_dump();
    Logging::printf(" base 0x%lx", _base);
  }

  /**
   * Initialize an MBI from the hip.
   */
  bool init_mbi(unsigned long mbi,  unsigned long &rip)
  {

    MessageHostOp msg1(MessageHostOp::OP_GUEST_MEM, 0);
    if (!(_mb.bus_hostop.send(msg1))) Logging::panic("could not find base address %x\n", 0);
    char *physmem = msg1.ptr;
    unsigned long memsize = msg1.len;
    unsigned long offset = 1 << 20;
    Mbi *m = reinterpret_cast<Mbi*>(physmem + mbi);
    memset(m, 0, sizeof(*m));

    bool res = false;
    for (unsigned modcount = 1; ; modcount++)
      {
	MessageHostOp msg(modcount, physmem + offset);
	if (!(_mb.bus_hostop.send(msg)) || !msg.size)  break;
	Logging::printf("\tmodule %x start %p+%lx cmdline %20s\n", modcount, msg.start, msg.size, msg.cmdline);

	switch(modcount)
	  {
	  case 0:  Logging::panic("invald module to start");
	  case 1:
	    if (Elf::decode_elf(physmem + offset, physmem, rip, offset, memsize, 0)) return res;
	    offset = (offset + 0xffful) & ~0xffful;
	    res = true;
	    m->cmdline = msg.cmdline - physmem;
	    m->flags |= MBI_FLAG_CMDLINE;
	    // keep space for fiasco
	    offset += 0x200000;
	    break;
	  default:
	    {
	      m->flags |= MBI_FLAG_MODS;
	      m->mods_addr = reinterpret_cast<char *>(m + 1) - physmem;
	      Module *mod = reinterpret_cast<Module *>(physmem + m->mods_addr) + m->mods_count;
	      m->mods_count++;
	      mod->mod_start = msg.start - physmem;
	      mod->mod_end = mod->mod_start + msg.size;
	      mod->string = msg.cmdline - physmem;
	      offset += ((msg.size + msg.cmdlen + 0xffful) & ~0xffful);
	    }
	    break;
	  }
	// 
	
      }
    MbiMmap mymap[] = {{20, 0, 0xa0000, 0x1},
		       {20, 1<<20, memsize - (1<<20), 0x1}};
    m->mem_lower = 640;
    m->mem_upper = (memsize >> 10) - 1024;
    m->mmap_addr  = offset;
    m->mmap_length = sizeof(mymap);
    m->flags |= MBI_FLAG_MMAP | MBI_FLAG_MEM;
    memcpy(physmem + m->mmap_addr, mymap, m->mmap_length);
    return true;
  };


 public:
  bool  receive(MessageExecutor &msg)
  {
    assert(msg.cpu->head.pid == 33);
    if (msg.cpu->cs.base + msg.cpu->eip != _base)  return false;
    Logging::printf(">\t%s mtr %x rip %x ilen %x cr0 %x efl %x\n", __PRETTY_FUNCTION__, 
		    msg.cpu->head.mtr.value(), msg.cpu->eip, msg.cpu->inst_len, msg.cpu->cr0, msg.cpu->efl);

    unsigned long rip;
    unsigned long mbi = 0x10000;

    if (!init_mbi(mbi, rip))  return false;
    msg.cpu->eip      = rip;
    msg.cpu->eax      = 0x2badb002;
    msg.cpu->ebx      = mbi;
    msg.cpu->cr0      = 0x11;
    msg.cpu->cs.ar    = 0xc9b;
    msg.cpu->cs.limit = 0xffffffff;
    msg.cpu->cs.base  = 0x0;
    msg.cpu->ss.ar    = 0xc93;
    msg.cpu->efl      = 2;
    msg.cpu->ds.ar = msg.cpu->es.ar = msg.cpu->fs.ar = msg.cpu->gs.ar = msg.cpu->ss.ar;
    msg.cpu->ld.ar    = 0x1000;
    msg.cpu->tr.ar    = 0x8b;
    msg.cpu->ss.base  = msg.cpu->ds.base  = msg.cpu->es.base  = msg.cpu->fs.base  = msg.cpu->gs.base  = msg.cpu->cs.base;
    msg.cpu->ss.limit = msg.cpu->ds.limit = msg.cpu->es.limit = msg.cpu->fs.limit = msg.cpu->gs.limit = msg.cpu->cs.limit;
    msg.cpu->tr.limit = msg.cpu->ld.limit = msg.cpu->gd.limit = msg.cpu->id.limit = 0xffff;
    msg.cpu->head.mtr = Mtd(MTD_ALL, 0);
    msg.vcpu->hazard |= VirtualCpuState::HAZARD_CRWRITE;
    return true;
  }

  MultibootExecutor(Motherboard &mb, unsigned long base) : _mb(mb), _base(base) {}
};

PARAM(multiboot,
      {
	mb.bus_executor.add(new MultibootExecutor(mb, argv[0]),  &MultibootExecutor::receive_static, 33);
      },
      "multiboot:eip - create a executor that supports multiboot",
      "Example:  'multiboot:0xfffffff0'",
      "If the eip is reached, the CPU is initilizes and the modules are requested from sigma0.");
