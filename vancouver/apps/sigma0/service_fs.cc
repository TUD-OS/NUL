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

class Service_fs {
  Hip * hip;
  unsigned _rights;

public:
  Service_fs(Motherboard &mb, bool readonly = true ) : hip(mb.hip()), _rights(readonly ? DESC_RIGHT_R : DESC_RIGHTS_ALL) {}

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

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap) {
    unsigned op, len;
    check1(EPROTO, input.get_word(op));

    switch (op) {
    case ParentProtocol::TYPE_OPEN:
    case ParentProtocol::TYPE_CLOSE:
      return ENONE;
    case FsProtocol::TYPE_GET_FILE_INFO:
      {
        Hip_mem * hmem = get_file(input.get_zero_string(len));
        check1(EPROTO, !hmem || !len);
        utcb << hmem->size;
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

PARAM(service_romfs,
      Service_fs *t = new Service_fs(mb);
      MessageHostOp msg(MessageHostOp::OP_REGISTER_SERVICE, reinterpret_cast<unsigned long>(StaticPortalFunc<Service_fs>::portal_func), reinterpret_cast<unsigned long>(t));
      msg.ptr = const_cast<char *>("/fs/rom");
      if (!mb.bus_hostop.send(msg))
        Logging::panic("registering the service failed");
      Logging::printf("start service - romfs\n");
      ,
      "romfs - instanciate a file service providing the boot files");

