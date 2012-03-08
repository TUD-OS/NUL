/*
 * Copyright (C) 2011, 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
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

#include <nul/parent.h>

struct EchoProtocol : public GenericProtocol {

  enum {
    TYPE_ECHO = ParentProtocol::TYPE_GENERIC_END,
    TYPE_GET_LAST,
  };

  unsigned echo(Utcb &utcb, unsigned value) {
    return call_server_drop(init_frame(utcb, TYPE_ECHO) << value);
  }

  unsigned get_last(Utcb &utcb, unsigned &last_val) {
    unsigned res = call_server_keep(init_frame(utcb, TYPE_GET_LAST));
    if (res == ENONE)
      last_val = utcb.msg[1];
    utcb.drop_frame();
    return res;
  }

  explicit EchoProtocol(CapAllocator *a, unsigned instance=0, bool blocking = true)
    : GenericProtocol("echo", instance,
                      a->alloc_cap(EchoProtocol::CAP_SERVER_PT + Global::hip.cpu_count()),
                      blocking) {}
  void close() {
    GenericProtocol::close(*BaseProgram::myutcb(), EchoProtocol::CAP_SERVER_PT + Global::hip.cpu_count());
  }
};
