/*
 * Copyright (C) 2011, Michal Sojka <sojka@os.inf.tu-dresden.de>
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

//#include <nul/capalloc.h>
#include <nul/parent.h>

struct EchoProtocol : public GenericProtocol {

  enum {
    TYPE_ECHO = ParentProtocol::TYPE_GENERIC_END,
    TYPE_GET_LAST,
  };

  unsigned echo(Utcb &utcb, unsigned value) {
    return call_server(init_frame(utcb, TYPE_ECHO) << value, true);
  }

  unsigned get_last(Utcb &utcb) {
    return call_server(init_frame(utcb, TYPE_GET_LAST), true);
  }

  explicit EchoProtocol(unsigned cap_base, unsigned instance=0, bool blocking = true)
    : GenericProtocol("echo", instance, cap_base, blocking) {}
};
