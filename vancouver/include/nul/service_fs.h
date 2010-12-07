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

#include "nul/generic_service.h"

/**
 */
struct FsProtocol : public GenericProtocol {

  enum {
    TYPE_GET_FILE_MAPPED = ParentProtocol::TYPE_GENERIC_END,
    TYPE_GET_FILE_INFO,
  };

  struct dirent {
    unsigned long long size;
    char const * name;
  };

  unsigned get_file_info(Utcb &utcb, const char * name, struct dirent & dirent) {
    unsigned res = call_server(init_frame(utcb, TYPE_GET_FILE_INFO) << name, false);
    utcb >> dirent.size;
    dirent.name = name;
    utcb.drop_frame();
    return res;
  }

  unsigned get_file_map(Utcb &utcb, const char * name, unsigned long addr, unsigned order, unsigned long & offset) {
    assert (!(addr & ((1 << Utcb::MINSHIFT) - 1)));
    assert (!(order >= (1 << 5)));
    unsigned res = call_server(init_frame(utcb, TYPE_GET_FILE_MAPPED) << name << Crd(addr >> Utcb::MINSHIFT, order, DESC_MEM_ALL), false);
    utcb >> offset;
    utcb.drop_frame();
    return res;
  }

  FsProtocol(unsigned cap_base, const char * name, unsigned instance=0) : GenericProtocol(name, instance, cap_base, true) {
    assert(!strncmp("fs", name, 2));
  }
};
