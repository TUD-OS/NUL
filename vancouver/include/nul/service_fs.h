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
    TYPE_GET_FILE_COPY,
    TYPE_GET_FILE_INFO,
  };

  enum type {
    FH_REGULAR_FILE = 0x10,
  };

  struct dirent {
    enum type type;
    unsigned size;
    unsigned name_len;
    char name[128];
  };

  unsigned get_file_info(Utcb &utcb, const char * name, struct dirent & dirent) {
    unsigned res = call_server(init_frame(utcb, TYPE_GET_FILE_INFO) << name, false);
    utcb >> dirent;
    utcb.drop_frame();
    return res;
  }

  unsigned get_file_copy(Utcb &utcb, const char * name, unsigned long addr, unsigned long size) {
    unsigned offset = addr & ((1UL << Utcb::MINSHIFT) - 1);
    unsigned order  = (addr + size + 0xFFFU) / 0x1000U;
    order = Cpu::bsr(order | 1) == Cpu::bsf(order | 1 << (8 * sizeof(unsigned) - 1)) ? Cpu::bsr(order | 1) : Cpu::bsr(order | 1) + 1;
    //Logging::printf("offset %x size %lx order=%x addr=%lx\n", offset, size, order, addr >> Utcb::MINSHIFT);
    return call_server(init_frame(utcb, TYPE_GET_FILE_COPY) << name << offset << size << Utcb::TypedIdentifyCap(addr >> Utcb::MINSHIFT, (order << 7) | DESC_MEM_ALL, false), true);
  }

  unsigned get_file_map(Utcb &utcb, const char * name, unsigned long addr, unsigned order, unsigned long & offset) {
    if (addr & ((1 << Utcb::MINSHIFT) - 1)) return EPROTO;
    if (order >= (1 << 5)) return EPROTO;
    unsigned res = call_server(init_frame(utcb, TYPE_GET_FILE_MAPPED) << name << Crd(addr >> Utcb::MINSHIFT, order, DESC_MEM_ALL), false);
    utcb >> offset;
    utcb.drop_frame();
    return res;
  }

  FsProtocol(unsigned cap_base, unsigned instance=0) : GenericProtocol("fs/rom", instance, cap_base, true) {}
};
