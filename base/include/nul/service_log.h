/** @file
 * Client part of the log protocol.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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

#include "parent.h"

/**
 * Missing: handle very-long strings
 */
struct LogProtocol : public GenericProtocol {
  enum {
    TYPE_LOG = ParentProtocol::TYPE_GENERIC_END,
  };
  unsigned log(Utcb &utcb, const char *line) {
    return call_server(init_frame(utcb, TYPE_LOG) << Utcb::String(line), true);
  }

  LogProtocol(unsigned cap_base, unsigned instance=0) : GenericProtocol("log", instance, cap_base, true) {}
};
