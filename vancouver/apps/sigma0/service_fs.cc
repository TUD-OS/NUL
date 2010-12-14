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
#include "nul/region.h"
#include "nul/baseprogram.h"

class Service_fs {
  Hip * hip;
  unsigned _rights;
  RegionList<8> * cap_range;

  static const unsigned long RECV_WINDOW_SIZE = (1 << 22);
  static char * backup_page;

public:
  Service_fs(Motherboard &mb, unsigned cap_start, unsigned cap_size, bool readonly = true )
    : hip(mb.hip()), _rights(readonly ? DESC_RIGHT_R : DESC_RIGHTS_ALL)
  {
    cap_range = new RegionList<8>;
    cap_range->add(Region(cap_start, cap_size));
    backup_page = new (0x1000) char [0x1000];
  }

  inline unsigned alloc_cap(unsigned num = 1, unsigned align = 0) { return cap_range->alloc(num, align); }
  unsigned alloc_crd() { assert(!"rom fs don't keep mappings and should never ask for new ones"); }

  Hip_mem * get_file(char const * text) {
    Hip_mem *hmem;

    for (int i=0; hip->mem_size && i < (hip->length - hip->mem_offs) / hip->mem_size; i++)
    {
      hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(hip) + hip->mem_offs + i * hip->mem_size);
      if (hmem->type != -2 || !hmem->size || !hmem->aux) continue;
      if (hmem->size == 1) continue; //skip configuration file
      char * virt_aux = reinterpret_cast<char *>(hmem->aux);
      if (strcmp(virt_aux, text)) continue;
      return hmem;
    }

    return 0;
  }

  static void portal_pagefault(Service_fs *tls, Utcb *utcb) __attribute__((regparm(0)))
  {
    //XXX sanity checks ?  whether stack and utcb is reasonable ...
    Utcb * utcb_wo = BaseProgram::myutcb(utcb->esp);

    //Logging::printf("worker utcb %p region %x order %x\n", utcb_wo, utcb_wo->get_nested_frame().get_crd() >> 12, (utcb_wo->get_nested_frame().get_crd() >> 7) & 0x1f);
    unsigned long region_start = utcb_wo->get_nested_frame().get_crd() >> 12;
    unsigned long region_end = region_start + ((utcb_wo->get_nested_frame().get_crd() >> 7) & 0x1f);

    if ((region_start <= (utcb->qual[1] & ~0xffful)) && ((utcb->qual[1] & ~0xffful) < region_end))
      Logging::panic("got #PF at %llx eip %x esp %x error %llx\n", utcb->qual[1], utcb->eip, utcb->esp, utcb->qual[0]);

    utcb_wo->head.res = 1; //flag abort
    utcb->head.mtr = 0;
    utcb->mtd = 0;
    BaseProgram::add_mappings(utcb, reinterpret_cast<unsigned long>(backup_page), 0x1000, (utcb->qual[1] & ~0xffful) | MAP_MAP, DESC_MEM_ALL);
    asmlinkage_protect("g"(tls), "g"(utcb));
  }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap) {
    unsigned op, len;
    check1(EPROTO, input.get_word(op));

    switch (op) {
    case FsProtocol::TYPE_GET_FILE_INFO:
      {
        Hip_mem * hmem = get_file(input.get_zero_string(len));
        check1(EPROTO, !hmem || !len);
        utcb << hmem->size;
      }
      return ENONE;
    case FsProtocol::TYPE_GET_FILE_COPIED:
      {
        Hip_mem * hmem = get_file(input.get_zero_string(len));
        check1(EPROTO, !hmem || !len);

        unsigned long long foffset;
        check1(EPROTO, input.get_word(foffset));
        check1(ERESOURCE, foffset > hmem->size);

        //XXX handle multiple map items !!!
        //input.dump_typed_items();

        unsigned long addr = input.received_item();
        check1(EPROTO, !(addr >> 12));

        unsigned long long csize = hmem->size - foffset;
        if (RECV_WINDOW_SIZE < csize) csize = RECV_WINDOW_SIZE;
        if ((1ULL << (12 + ((addr >> 7) & 0x1f))) < csize) csize = 1ULL << (12 + ((addr >> 7) & 0x1f));

        utcb.head.res = 0; //set pf indicator to off
        unsigned long _addr = addr & ~0xffful;
        unsigned long long _size = csize;
        while (!utcb.head.res && _size && _size <= csize) {
          memcpy(reinterpret_cast<void *>(_addr), reinterpret_cast<void *>(hmem->addr + foffset), 0x1000); 
          _addr += 0x1000; foffset += 0x1000; _size -= 0x1000;
        }
        //Logging::printf("abort %x addr %lx utcb %p _size %llx foffset %llx\n", utcb.head.res, addr, &utcb, _size, foffset);
        if (utcb.head.res) return ERESOURCE; //got pf
      }
      return ENONE;
    case FsProtocol::TYPE_GET_FILE_MAPPED:
      {
        Hip_mem * hmem = get_file(input.get_zero_string(len));
        check1(EPROTO, !hmem || !len);

        unsigned long long addr = hmem->addr, order;
        unsigned long long size = (hmem->size + 0xfff) & ~0xffful;
        unsigned long hotspot = 0;
        unsigned long virt_size = 1 << (Cpu::bsr(size | 1) == Cpu::bsf(size | 1 << (8 * sizeof(unsigned) - 1)) ? Cpu::bsr(size | 1) : Cpu::bsr(size | 1) + 1);

        if ((addr & ~(virt_size - 1)) + virt_size > addr + size) {
          hotspot = addr & (virt_size - 1);
          utcb << hotspot;
        } else {
          hotspot = addr & (1 << Cpu::bsr(virt_size - size)) - 1;
          utcb << hotspot;
        }
        //Logging::printf("virt_size=%lx ('hotspot'=%lx !<= restsize=%llx) \n",virt_size, hotspot, virt_size - size);
        while (size) {
          order = Cpu::minshift(addr | hotspot, size);

          //Logging::printf("typed %x addr %llx order %llx size %llx hotspot %lx msg %p START %x &msg[START]=%p item_start %p\n",
          //  utcb.head.typed, addr, order, size, hotspot, utcb.msg, utcb.msg[Utcb::STACK_START], &utcb.msg[Utcb::STACK_START], utcb.item_start());

          Utcb::TypedMapCap value = Utcb::TypedMapCap(addr >> Utcb::MINSHIFT, ((order - 12) << 7) | DESC_TYPE_MEM | _rights);
          utcb.head.typed++;
          value.fill_words(utcb.item_start(), hotspot | MAP_MAP); //MAP_HBIT);
          addr    += 1 << order;
          size    -= 1 << order;
          hotspot += 1 << order;
        }
      }
      return ENONE;
    default:
      return EPROTO;
    }
  }

};

char * Service_fs::backup_page;

PARAM(service_romfs,
      Service_fs *t = new Service_fs(mb, alloc_cap(15 + mb.hip()->cpu_count() * 16), 15 + mb.hip()->cpu_count() * 16);
      MessageHostOp msg(t, "/fs/rom", reinterpret_cast<unsigned long>(StaticPortalFunc<Service_fs>::portal_func), false);
      msg.portal_pf = reinterpret_cast<unsigned long>(Service_fs::portal_pagefault);
      msg.excbase = t->alloc_cap(16 * mb.hip()->cpu_count(), 4);
      msg.excinc  = 4;
      if (!msg.excbase || !mb.bus_hostop.send(msg))
        Logging::panic("registering the service failed");
      Logging::printf("start service - romfs\n");
      ,
      "romfs - instanciate a file service providing the boot files");

