/*
 * File service interface.
 *
 * Copyright (C) 2010, Alexander Boettcher <boettcher@tudos.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "nul/motherboard.h"
#include "nul/generic_service.h"
#include "nul/service_fs.h"
#include "nul/baseprogram.h"

class Service_fs {
protected:

  Hip * hip;
  unsigned _rights;

  static const unsigned long RECV_WINDOW_SIZE = (1 << 22);
  static char * backup_page;

public:
  virtual bool get_file(char const * text, Hip_mem &out) = 0;

  Service_fs(Motherboard &mb, bool readonly = true )
    : hip(mb.hip()), _rights(readonly ? DESC_RIGHT_R : DESC_RIGHTS_ALL)
  {
    backup_page = new (0x1000) char [0x1000];
  }

  unsigned alloc_crd() { Logging::panic("rom fs don't keep mappings and should never ask for new ones"); }

  static void portal_pagefault(Service_fs *tls, Utcb *utcb) __attribute__((regparm(0)))
  {
    //XXX sanity checks ?  whether stack and utcb is reasonable ...
    Utcb * utcb_wo = BaseProgram::myutcb(utcb->esp);

    //Logging::printf("worker utcb %p region %x order %x\n", utcb_wo, utcb_wo->get_nested_frame().get_crd() >> 12, (utcb_wo->get_nested_frame().get_crd() >> 7) & 0x1f);
    unsigned long region_start = utcb_wo->get_nested_frame().get_crd() >> 12;
    unsigned long region_end = region_start + ((utcb_wo->get_nested_frame().get_crd() >> 7) & 0x1f);

    if ((region_start <= (utcb->qual[1] & ~0xffful)) && ((utcb->qual[1] & ~0xffful) < region_end))
      Logging::panic("got #PF at %llx eip %x esp %x error %llx\n", utcb->qual[1], utcb->eip, utcb->esp, utcb->qual[0]);

    utcb_wo->head.crd_translate = 1; //flag abort
    utcb->head.mtr = 0;
    utcb->mtd = 0;
    utcb->msg[0] = utcb->add_mappings(reinterpret_cast<unsigned long>(backup_page), 0x1000, (utcb->qual[1] & ~0xffful) | MAP_MAP, DESC_MEM_ALL);
    assert(utcb->msg[0] == 0); //never fails, msg[0] not required for anything
    asmlinkage_protect("g"(tls), "g"(utcb));
  }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid) {
    unsigned op, len;
    check1(EPROTO, input.get_word(op));

    switch (op) {
    case FsProtocol::TYPE_INFO:
      {
        Hip_mem hmem;
        if (get_file(input.get_zero_string(len), hmem)) {
          utcb << hmem.size;
        } else {
          return EPROTO;
        }
      }
      return ENONE;
    case FsProtocol::TYPE_COPY:
      {
        Hip_mem hmem;
        if (not get_file(input.get_zero_string(len), hmem))
          return EPROTO;

        unsigned long long foffset;
        check1(EPROTO, input.get_word(foffset));
        check1(ERESOURCE, foffset > hmem.size);

        //XXX handle multiple map items !!!
        //input.dump_typed_items();

        unsigned long addr = input.received_item();
        check1(EPROTO, !(addr >> 12));

        unsigned long long csize = hmem.size - foffset;
        if (RECV_WINDOW_SIZE < csize) csize = RECV_WINDOW_SIZE;
        if ((1ULL << (12 + ((addr >> 7) & 0x1f))) < csize) csize = 1ULL << (12 + ((addr >> 7) & 0x1f));

        unsigned tmp_crd = utcb.head.crd_translate; //XXX find better place for pf indicator ?
        utcb.head.crd_translate = 0; //set pf indicator to off

        unsigned long _addr = addr & ~0xffful;
        unsigned long long _size = csize;
        while (!utcb.head.crd_translate && _size && _size <= csize) {
          memcpy(reinterpret_cast<void *>(_addr), reinterpret_cast<void *>(hmem.addr + foffset), _size > 0x1000U ? 0x1000 : _size);
          _addr += 0x1000; foffset += 0x1000; _size -= 0x1000;
        }
        // Check whether our pagefault handler signaled us an abort.
        if (utcb.head.crd_translate) {
          Logging::printf("region %#lx-%#llx \n pf addr %lx utcb %p _size %llx foffset %llx - pfoffset=%#lx\n\n", addr, csize, _addr - 0x1000, &utcb, _size + 0x1000, foffset - 0x1000, ((_addr - (addr & ~0xffful)) - 0x1000UL));
          utcb.head.crd_translate = tmp_crd;
          utcb << ((_addr - (addr & ~0xffful)) - 0x1000UL);
          return FsProtocol::EPAGEFAULT; //got pf
        } else
          utcb.head.crd_translate = tmp_crd;
      }
      return ENONE;
    default:
      return EPROTO;
    }
  }

};

class Service_ModuleFs : public Service_fs {
public:
  bool get_file(char const * text, Hip_mem &out)
  {
    Hip_mem *hmem;
    //unsigned len;

    for (int i=0; hip->mem_size && i < (hip->length - hip->mem_offs) / hip->mem_size; i++)
      {
        hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(hip) + hip->mem_offs + i * hip->mem_size);
        if (hmem->type != -2 || !hmem->size || !hmem->aux) continue;
        char * virt_aux = reinterpret_cast<char *>(hmem->aux);
        //len = strcspn(virt_aux, " \t\r\n\f");
        //      if (len >= 10 && !strncmp(virt_aux + len - 10, ".nulconfig", 10)) continue; //skip configuration files for security/spying reasons
        if (strcmp(virt_aux, text)) continue;

        out = *hmem;
        return true;
      }

    return false;
  }
public:
  Service_ModuleFs(Motherboard &mb, bool readonly = true )
    : Service_fs(mb, readonly)
  {}
};

#include <service/elf.h>

class Service_ElfFs : public Service_fs {
  bool get_file(char const * text, Hip_mem &out)
  {
    const char *file = reinterpret_cast<char *>(hip->get_mod(0)->addr);
    const eh32 *eh32 = reinterpret_cast<const struct eh32 *>(file);
    assert(not Elf::is_not_elf(eh32, hip->get_mod(0)->size));
    
    if (eh32->e_shstrndx >= eh32->e_shnum)
      // No string table
      return NULL;

    const sh32 *sh_strtab = reinterpret_cast<const struct sh32 *>(file + eh32->e_shoff + eh32->e_shentsize*eh32->e_shstrndx);
    const char *strtab = reinterpret_cast<const char *>(file + sh_strtab->sh_offset);

    // String table always begins with a zero byte.
    assert(*strtab == 0);

    for (unsigned i = 0; i < eh32->e_shnum; i++) {
      const  sh32 *sh32 = reinterpret_cast<const struct sh32 *>(file + eh32->e_shoff +
                                                                eh32->e_shentsize*i);
      const char *name = strtab + sh32->sh_name;

      if ((strncmp(".boot.", name, sizeof(".boot.") - 1) == 0) and
          (strcmp(name + sizeof(".boot.") - 1, text) == 0)) {
        out.type = -2;
        out.addr = reinterpret_cast<mword>(file + sh32->sh_offset);
        out.size = sh32->sh_size;
        return true;
      }
    }
    return false;
  }

public:
  Service_ElfFs(Motherboard &mb, bool readonly = true )
    : Service_fs(mb, readonly)
  {}
};

char * Service_fs::backup_page;

PARAM_HANDLER(service_romfs,
	      "romfs - instanciate a file service providing the boot files")
{
  Service_ModuleFs *t = new Service_ModuleFs(mb);
  MessageHostOp msg(t, "/fs/rom", reinterpret_cast<unsigned long>(StaticPortalFunc<Service_ModuleFs>::portal_func), 0, false);
  msg.portal_pf = reinterpret_cast<unsigned long>(Service_ModuleFs::portal_pagefault);
  msg.excbase = alloc_cap_region(16 * mb.hip()->cpu_count(), 4);
  msg.excinc  = 4;
  if (!msg.excbase || !mb.bus_hostop.send(msg))
    Logging::panic("registering the service failed");
}

PARAM_HANDLER(service_embeddedromfs,
	      "embeddedromfs")
{
  Service_ElfFs *t = new Service_ElfFs(mb);
  MessageHostOp msg(t, "/fs/embedded", reinterpret_cast<unsigned long>(StaticPortalFunc<Service_ElfFs>::portal_func), 0, false);
  msg.portal_pf = reinterpret_cast<unsigned long>(Service_ElfFs::portal_pagefault);
  msg.excbase = alloc_cap_region(16 * mb.hip()->cpu_count(), 4);
  msg.excinc  = 4;
  if (!msg.excbase || !mb.bus_hostop.send(msg))
    Logging::panic("registering the service failed");
}

