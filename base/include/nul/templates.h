/** @file
 * NUL helper.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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
/**
 * This template converts from static receive to member functions.
 */
template<typename Y>
class StaticReceiver : public Device
{
public:
  template<class M>
  static bool receive_static(Device *o, M& msg) { return static_cast<Y*>(o)->receive(msg); }
  StaticReceiver() : Device(__PRETTY_FUNCTION__) {};
};


/**
 * This template provides easy access to the Discovery Bus.
 */
template<typename Y> class DiscoveryHelper
{
protected:
  /*
   * Write a string to a resource.
   */
  bool discovery_write_st(const char *resource, unsigned offset, const void *value, unsigned count) {
    MessageDiscovery msg(resource, offset, value, count);
    return static_cast<Y*>(this)->_mb.bus_discovery.send(msg);
  }


  /**
   * Write a dword or less than it.
   */
  bool discovery_write_dw(const char *resource, unsigned offset, unsigned value, unsigned count = 4)  {
    return discovery_write_st(resource, offset, &value, count);
  }


  /**
   * Read a dword.
   */
  bool discovery_read_dw(const char *resource, unsigned offset, unsigned &value)  {
    MessageDiscovery msg(resource, offset, &value);
    return static_cast<Y*>(this)->_mb.bus_discovery.send(msg);
  }

  /**
   * Return the length of an ACPI table or minlen if it is smaller.
   */
  unsigned discovery_length(const char *resource, unsigned minlen) {
    unsigned res;
    if (!discovery_read_dw(resource, 4, res) || res < minlen) return minlen;
    return res;
  }


public:
  static bool  discover(Device *o, MessageDiscovery &msg) {
    if (msg.type != MessageDiscovery::DISCOVERY) return false;
    static_cast<Y*>(o)->discovery();
    return true;
  }
};
