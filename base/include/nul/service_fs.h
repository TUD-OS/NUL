/** @file
 * Client part of the fs protocol.
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
#pragma once

#include <nul/types.h>
#include <service/string.h>
#include "parent.h"
#include <nul/baseprogram.h>

/**
 */
struct FsProtocol : public GenericProtocol {

  enum opcode {
    TYPE_INFO = ParentProtocol::TYPE_GENERIC_END,
    TYPE_COPY,
    TYPE_MAP,
    TYPE_CREATE,
    TYPE_WRITE,
    EPAGEFAULT,
  };

  struct dirent {
    unsigned long long size;
    char const * name;

    dirent() : size(0), name(NULL) {}
  };

  struct File;

  // Parse a string of the form "proto://foo/bar" into foo/bar (return
  // value) and "proto". Return value shares structure with url. This
  // function does not trim or remove spaces (it's your job!).  If
  // string is not parsable or proto buffer is to small, return
  // NULL. Needed size is returned in proto_len.
  static const char *
  parse_file_name(const char *url, char *proto, size_t &proto_len)
  {
    const char *name = strstr(url, "://");
    if (name == NULL) { return NULL; }

    if (size_t(name - url + 1) > proto_len) {
      proto_len = name - url + 1;
      return NULL;
    }

    memcpy(proto, url, name - url);
    proto[name - url] = 0;

    return name + 3;
  }

  /*
   * Create a new file. Communication cap will be mapped to cap provided in File object - cap valid only for cpu of calling thread is from - to be fixed XXX.
   */
  unsigned create(Utcb &utcb, File &file, const char * name, unsigned long name_len = ~0UL) {
    unsigned res = call_server_keep(init_frame(utcb, TYPE_CREATE) << Utcb::String(name, name_len) << Crd(file.file_cap, 0, DESC_CAP_ALL));
    unsigned recv_portal = Utcb::Frame(&utcb, sizeof(utcb.msg)/sizeof(unsigned)).received_item();
    utcb.drop_frame();
    if (recv_portal != Crd(file.file_cap, 0, DESC_CAP_ALL).value()) return EPROTO; //received cap doesn't match the expected one
    return res;
  }

  unsigned get(Utcb &utcb, File &file, const char * name, unsigned long name_len = ~0UL) {
    file.name = name;
    file.name_len = name_len;
    return ENONE;
  }

  FsProtocol(unsigned cap_base, const char * name, unsigned instance=0) : GenericProtocol(name, instance, cap_base, true) {
    assert(!strncmp("fs", name, 2));
  }

  struct File {
    FsProtocol &fs_obj;
    cap_sel file_cap;
    const char * name;
    unsigned long name_len;

    File(FsProtocol &fs_o, cap_sel file) : fs_obj(fs_o), file_cap(file) {}

    unsigned get_info(Utcb &utcb, dirent & dirent) {
      unsigned res = fs_obj.call_server(init_frame_noid(utcb, TYPE_INFO) << Utcb::String(name, name_len), false);
      dirent.size = 0; utcb >> dirent.size;
      dirent.name = name;
      utcb.drop_frame();
      return res;
    }

    /*
     * Caller maps its own memory to the fileservice and the fileservice
     * copies the data into the provided memory.  Service has to take
     * care that memory may be revoked by the client at anytime!
     */
    unsigned copy(Utcb &utcb, void *addr, unsigned long addr_size,
                  unsigned long long file_offset = 0) {
      unsigned long local_offset = 0;
      unsigned res = 0, order;
      unsigned long addr_int = reinterpret_cast<unsigned long>(addr);
      assert (!(addr_int & 0xffful) && addr_size);

      //XXX support multiple send items 
      while(!res && addr_size) {
        order = Cpu::minshift(local_offset + addr_int, ((addr_size + 0xfffull) & ~0xfffull));
        assert (order >= 12);
        if (order > 22) order = 22;

        res = fs_obj.call_server(init_frame_noid(utcb, TYPE_COPY) << Utcb::String(name, name_len) << file_offset
                                 << Utcb::TypedMapCap((addr_int + local_offset) >> Utcb::MINSHIFT, Crd(0, order - 12, DESC_MEM_ALL).value()), false);
        if (res == EPAGEFAULT) {
          utcb.msg[0] = 0; //utcb/frame code assumes that msg[0] is zero in case of success if you use '>>' to get transferred data
          unsigned long pf;
          if (utcb >> pf) return ERESOURCE; //no hint was given what went wrong - abort
          if (pf > addr_size) return EPROTO; //service fs try to cheat us - damn boy - abort 
          Logging::printf("request mapping fs - addr=%#lx offset=%#lx pf=%#lx\n", addr_int, local_offset, pf);
          BaseProgram::request_mapping(reinterpret_cast<char *>(addr_int + local_offset + pf), 0x1000U, 0); //at least one page
          utcb.drop_frame();
          continue; //retry
        }
        utcb.drop_frame();

        file_offset += 1ULL << order; local_offset += 1UL << order;
        addr_size   -= (1ULL << order) > addr_size ? addr_size : (1ULL << order);
      }
      return res;
    }

    /*
     * Get a file mapped by the service. Client has to take care that memory can be revoked by the service at anytime !
     */
    unsigned map(Utcb &utcb, unsigned long addr, unsigned order, unsigned offset, unsigned & size) {
      assert (!(addr & ((1 << Utcb::MINSHIFT) - 1)));
      assert (!(order >= (1 << 5)));

      Logging::printf("crd %#x\n", Crd(addr >> Utcb::MINSHIFT, order, DESC_MEM_ALL).value());
      init_frame_noid(utcb, TYPE_MAP) << offset << order << Crd(addr >> Utcb::MINSHIFT, order - 12, DESC_MEM_ALL);
      unsigned res = call(utcb, file_cap, false, false); //don't drop frame (false), and use variable file as portal (false)
      utcb >> size;
      utcb.drop_frame();
      return res;
    }

    /*
     * Write content to a file. Memory get mapped to the fs service.
     */
    unsigned write(Utcb &utcb, void const *addr_void, unsigned long region_size)
    {
      unsigned res = 0;
      unsigned long addr = reinterpret_cast<unsigned long>(addr_void);
      assert (!(addr & 0xffful) && region_size);
      unsigned long size = (region_size + 0xfff) & ~0xffful;
      unsigned long hotspot = 0, map_size;

      while (size) {
        map_size = 1 << (Cpu::bsr(size | 1) == Cpu::bsf(size | 1 << (8 * sizeof(unsigned) - 1)) ? Cpu::bsr(size | 1) : Cpu::bsr(size | 1) + 1);

        if ((addr & ~(map_size - 1)) + map_size > addr + size || map_size == size)
          hotspot = addr & (map_size - 1);
        else
          hotspot = addr & ((1 << Cpu::bsr(map_size - size | 1)) - 1);

//      Logging::printf("map_size=%#lx(%lu) ('hotspot'=%lx !<= restsize=%lx) addr %#lx+%#lx(%p+%#lx) \n", map_size, map_size, hotspot, map_size - size, addr, size, addr_void, region_size);

        //start utcb frame
        init_frame_noid(utcb, TYPE_WRITE);
        while (size) {
          unsigned long order = Cpu::minshift(addr | hotspot, size);
   
          if ((hotspot + (1 << order)) > (1 << 22)) break; //we have only 4M receive windows currently

//        Logging::printf("typed %x addr %lx order %lx size %lx hotspot %lx msg %p START %x &msg[START]=%p item_start %p\n",
//          utcb.head.typed, addr, order, size, hotspot, utcb.msg, utcb.msg[Utcb::STACK_START], &utcb.msg[Utcb::STACK_START], utcb.item_start());

          Utcb::TypedMapCap value = Utcb::TypedMapCap(Crd(addr >> Utcb::MINSHIFT, order - 12, DESC_MEM_ALL), hotspot, MAP_MAP); //MAP_HBIT);
          utcb.head.typed++;
          value.fill_words(utcb.item_start());
          addr    += 1 << order;
          size    -= 1 << order;
          hotspot += 1 << order;

        }
        res = call(utcb, file_cap, true, false); //drop frame (true), and use variable file as portal (false)
        //end utcb frame
      }

      return res;
    }
  };
 
};

// EOF
