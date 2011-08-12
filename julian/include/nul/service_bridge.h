/*
 * Client part of the bridge protocol.
 *
 * Copyright (C) 2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include <nul/error.h>
#include <nul/parent.h>

struct BridgeProtocol : public GenericProtocol {

  enum {
    RING_BUFFER_SIZE = 4096,
  };

  enum {
    TYPE_GET_RING = ParentProtocol::TYPE_GENERIC_END,
  };

  void *get_ring(Utcb &utcb, size_t &len)
  {
    /* TODO */
    len = 0;
    return NULL;
  }

  BridgeProtocol(unsigned cap_base, unsigned instance=0) : GenericProtocol("bridge", instance, cap_base, true) {}
};

// EOF
