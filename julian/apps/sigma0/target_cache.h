// -*- Mode: C++

#pragma once

#include <service/string.h>
#include <service/net.h>

template <unsigned SIZE, typename T>
class TargetCache {
  unsigned cur;

  struct {
    EthernetAddr addr;
    T           *target;
  } _targets[SIZE];

public:

  void remember(EthernetAddr const &addr, T *port)
  {
    if (_targets[(cur - 1) % SIZE].addr == addr) {
      _targets[(cur - 1) % SIZE].target = port;
    } else {
      _targets[cur].addr    = addr;
      _targets[cur].target  = port;
      cur = (cur + 1) % SIZE;
    }
  }

  T *lookup(EthernetAddr const &addr)
  {
    if (not addr.is_multicast())
      for (unsigned i = 0; i < SIZE; i++)
        if (_targets[i].addr == addr)
          return _targets[i].target;
    return NULL;
  }

  TargetCache()
    : cur(0)
  {
    memset(_targets, 0, sizeof(_targets));
  }

};


// EOF
