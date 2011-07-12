// -*- Mode: C++ -*-
/** @file
 * Intel 82576 VF device model.
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <service/net.h>

class Mta {
  uint32 _bits[128];

public:
  
  static uint16 hash(EthernetAddr const &addr)
  {
    return 0xFFF & (((addr.byte[4] >> 4)
		     | static_cast<uint16>(addr.byte[5]) << 4));
  }

  bool includes(EthernetAddr const &addr) const
  {
    uint16 h = hash(addr);
    return (_bits[(h >> 5) & 0x7F] & (1 << (h & 0x1F))) != 0;
  }

  void set(uint16 hash) { _bits[(hash >> 5) & 0x7F] |= 1 << (hash&0x1F); }
  void clear() { memset(_bits, 0, sizeof(_bits)); }

  Mta() : _bits() { }
};

// EOF
