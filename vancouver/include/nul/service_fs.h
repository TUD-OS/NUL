/*
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

#include <nul/generic_service.h>
#include <service/string.h>

/**
 */
struct FsProtocol : public GenericProtocol {

  enum {
    TYPE_GET_FILE_MAPPED = ParentProtocol::TYPE_GENERIC_END,
    TYPE_GET_FILE_INFO,
    TYPE_GET_FILE_COPIED,
  };

  struct dirent {
    unsigned long long size;
    char const * name;

    dirent() : size(0), name(NULL) {}
  };

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

    if ((name - url + 1) > proto_len) {
      proto_len = name - url + 1;
      return NULL;
    }

    memcpy(proto, url, name - url);
    proto[name - url] = 0;

    return name + 3;
  }

  unsigned get_file_info(Utcb &utcb, dirent & dirent, const char * name, unsigned long name_len = ~0UL) {
    unsigned res = call_server(init_frame(utcb, TYPE_GET_FILE_INFO) << Utcb::String(name, name_len), false);
    dirent.size = 0; utcb >> dirent.size;
    dirent.name = name;
    utcb.drop_frame();
    return res;
  }

  /*
   * Get a file mapped by the service. Client has to take care that memory can be revoked by the service at anytime !
   */
  unsigned get_file_map(Utcb &utcb, unsigned long addr, unsigned order, unsigned long & offset, const char * name, unsigned long name_len = ~0UL) {
    assert (!(addr & ((1 << Utcb::MINSHIFT) - 1)));
    assert (!(order >= (1 << 5)));
    unsigned res = call_server(init_frame(utcb, TYPE_GET_FILE_MAPPED) << Utcb::String(name, name_len) << Crd(addr >> Utcb::MINSHIFT, order, DESC_MEM_ALL), false);
    utcb >> offset;
    utcb.drop_frame();
    return res;
  }

  /*
   * Caller maps its own memory to the fileservice and the fileservice copies the data into the provided memory.
   * Service has to take care that memory may be revoked by the client at anytime!
   */
  unsigned get_file_copy(Utcb &utcb, void *addr, unsigned long addr_size, const char * name,
                         unsigned long name_len = ~0UL, unsigned long long file_offset = 0) {
    unsigned long local_offset = 0;
    unsigned res = 0, order;
    unsigned long addr_int = reinterpret_cast<unsigned long>(addr);
    assert (!(addr_int & 0xffful) && addr_size);

    //XXX support multiple send items 
    while(!res && addr_size) {
      order = Cpu::minshift(local_offset + addr_int, ((addr_size + 0xfffull) & ~0xfffull));
      assert (order >= 12);
      if (order > 22) order = 22;

      res = call_server(init_frame(utcb, TYPE_GET_FILE_COPIED) << Utcb::String(name, name_len) << file_offset
                     << Utcb::TypedMapCap((addr_int + local_offset) >> Utcb::MINSHIFT, Crd(0, order - 12, DESC_MEM_ALL).value()), true);
      file_offset += 1ULL << order; local_offset += 1UL << order;
      addr_size   -= (1ULL << order) > addr_size ? addr_size : (1ULL << order);
    }
    return res;
  }

  FsProtocol(unsigned cap_base, const char * name, unsigned instance=0) : GenericProtocol(name, instance, cap_base, true) {
    assert(!strncmp("fs", name, 2));
  }
};

// EOF
