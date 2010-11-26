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

  struct ClientData : public GenericClientData {};
  ClientDataStorage<ClientData> _storage;

  unsigned _rights;

public:
  Service_fs(Motherboard &mb, bool readonly = true ) : hip(mb.hip()), _rights(readonly ? DESC_RIGHT_R : DESC_RIGHTS_ALL) {}

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap) {
    ClientData *data = 0;
    unsigned res = ENONE;
    unsigned op;

    check1(EPROTO, input.get_word(op));

    switch (op) {
    case ParentProtocol::TYPE_OPEN:
      check1(res, res = _storage.alloc_client_data(utcb, data, input.received_cap()));
      free_cap = false;
      utcb << Utcb::TypedMapCap(data->identity);
      return res;
    case ParentProtocol::TYPE_CLOSE:
      check1(res, res = _storage.get_client_data(utcb, data, input.identity()));
      return _storage.free_client_data(utcb, data);
    case FsProtocol::TYPE_GET_FILE_INFO:
    case FsProtocol::TYPE_GET_FILE_COPY:
    case FsProtocol::TYPE_GET_FILE_MAPPED:
      {
        //check1(res, res = _storage.get_client_data(utcb, data, input.identity()));
        unsigned len;
        char *text = input.get_zero_string(len);
        check1(EPROTO, !text);

        for (int i=0; hip->mem_size && i < (hip->length - hip->mem_offs) / hip->mem_size; i++)
        {
          Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(hip) + hip->mem_offs + i * hip->mem_size);
          if (hmem->type != -2 || !hmem->size || !hmem->aux) continue;
          if (hmem->size == 1) continue; //skip configuration file
          char * virt_aux = reinterpret_cast<char *>(hmem->aux);
          if (strcmp(virt_aux, text)) continue;

          switch(op) {
            case FsProtocol::TYPE_GET_FILE_INFO:
              FsProtocol::dirent dirent;
              memset(&dirent, 0, sizeof(dirent));
              dirent.size = hmem->size;
              dirent.type = FsProtocol::type::FH_REGULAR_FILE;
	            dirent.name_len = strcspn(virt_aux, " \t\r\n\f");
              memcpy(dirent.name, virt_aux, dirent.name_len > sizeof(dirent.name) ? sizeof(dirent.name) - 1 : dirent.name_len);

              utcb << dirent;
              break;
            case FsProtocol::TYPE_GET_FILE_MAPPED:
              {
                unsigned long long addr = hmem->addr;
                unsigned long long size = (hmem->size + 0xfff) & ~0xffful;
                unsigned long long order;
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
              break;
            case FsProtocol::TYPE_GET_FILE_COPY:
              {
                //input.dump_typed_items();
                //XXX check for multiple regions
                unsigned long offset = 0;
                unsigned long size = 0;
                if (input.get_word(offset)) return EPROTO;
                if (input.get_word(size)) return EPROTO;
                if (size > hmem->size) return ERESOURCE;
                Crd region = input.translated_cap(1);
                if (!region.value() || !region.cap() || !(region.attr() & DESC_TYPE_MEM)) return ERESOURCE;
                if ((1UL << (region.order() + Utcb::MINSHIFT)) < (offset + size)) return ERESOURCE;

                void * rcv_addr = reinterpret_cast<void *>((region.cap() << Utcb::MINSHIFT) + offset);
                void * src_addr = reinterpret_cast<void *>(hmem->addr);
                if (!src_addr) return ERESOURCE;

//                Logging::printf("file_copy base=%x offset=%lx addr=%p hmem->addr %llx, size=%lx hmem->size=%llx %x mem_max=%x addr_virt %p %s crd=%x\n",
//                  region.cap() << Utcb::MINSHIFT, offset, rcv_addr, hmem->addr, size, hmem->size, region.order(), 1 << (region.order() + Utcb::MINSHIFT), src_addr, virt_aux, region.value());
                memcpy(rcv_addr, src_addr, size);
              }
              break;
            default:
              return EPROTO;
          }
          return ENONE;
        }
      }
      return ERESOURCE;
    default:
      Logging::printf("rom client %x\n", input.identity());

      input.dump_typed_items();

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

