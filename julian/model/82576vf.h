// -*- Mode: C++ -*-

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
