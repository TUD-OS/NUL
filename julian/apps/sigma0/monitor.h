// -*- Mode: C++ -*-

#pragma once

#include <nul/types.h>
#include <service/cpu.h>
#include <nul/motherboard.h>

enum {
  MONITOR_DATA_IN = 0,
  MONITOR_DATA_OUT,
  MONITOR_PACKET_IN,
  MONITOR_PACKET_OUT,
  MONITOR_MAX
};

class VnetMonitor {
public:

private:
  uint32 data[MONITOR_MAX];

public:

  template<unsigned V>
  void add(uint32 value)
  {
#ifndef BENCHMARK
    // uint64 newv = data[V] + value;
    // while (cmpxchg8b(&data[V], data[V], newv) != newv) {}
    asm ("lock; add %1, %0" : "+m" (data[V]) : "r" (value));
#endif
  }

  VnetMonitor(DBus<MessageHostOp> &bus_hostop) : data()
  {
    MessageHostOp m(MessageHostOp::OP_VIRT_TO_PHYS, reinterpret_cast<unsigned long>(data));
    if (!bus_hostop.send(m))
      Logging::panic("v2p failed");

    Logging::printf("VNET MONITOR %lx\n", m.phys);
  }
};


// EOF
