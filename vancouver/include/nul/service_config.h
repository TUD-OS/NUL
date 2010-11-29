/*
 * Client part of the config protocol.
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
struct ConfigProtocol : public GenericProtocol {

  enum {
    TYPE_START_CONFIG = ParentProtocol::TYPE_GENERIC_END,
    TYPE_START_CONFIG_TRANSLATE
  };

  unsigned start_config(Utcb &utcb, unsigned long addr, unsigned long size) {
    unsigned offset = addr & ((1UL << Utcb::MINSHIFT) - 1);
    unsigned order  = (addr + size + 0xFFFU) / 0x1000U;
    order = Cpu::bsr(order | 1) == Cpu::bsf(order | 1 << (8 * sizeof(unsigned) - 1)) ? Cpu::bsr(order | 1) : Cpu::bsr(order | 1) + 1;
    return call_server(init_frame(utcb, TYPE_START_CONFIG_TRANSLATE) << offset << size << Utcb::TypedIdentifyCap(addr >> Utcb::MINSHIFT, (order << 7) | DESC_MEM_ALL, false), true);
  }

  unsigned start_config(Utcb &utcb, char const * config) {
    return call_server(init_frame(utcb, TYPE_START_CONFIG) << config, true);
  }

  ConfigProtocol(unsigned cap_base, unsigned instance=0) : GenericProtocol("config", instance, cap_base, true) {}
};
