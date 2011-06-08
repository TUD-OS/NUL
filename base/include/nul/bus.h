/** @file
 * Bus infrastucture and generic Device class.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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

#include "service/logging.h"
#include "service/string.h"

/**
 * The generic Device used in generic bus transactions.
 */
class Device
{
  const char *_debug_name;
public:
  void debug_dump() {
    Logging::printf("\t%s\n", _debug_name);
  }
  Device(const char *debug_name) :_debug_name(debug_name) {}
};


/**
 * A bus is a way to connect devices.
 */
template <class M>
class DBus
{
  typedef bool (*ReceiveFunction)(Device *, M&);
  struct Entry
  {
    Device *_dev;
    ReceiveFunction _func;
  };

  unsigned long _debug_counter;
  unsigned _list_count;
  unsigned _list_size;
  struct Entry *_list;

  /**
   * To avoid bugs we disallow the copy constuctor.
   */
  DBus(const DBus<M> &bus) { Logging::panic("%s copy constructor called", __func__); }

  void set_size(unsigned new_size)
  {
    Entry *n = new Entry[new_size];
    memcpy(n, _list, _list_count * sizeof(*_list));
    if (_list)  delete [] _list;
    _list = n;
    _list_size = new_size;
  };
public:

  void add(Device *dev, ReceiveFunction func)
  {
    if (_list_count >= _list_size)
      set_size(_list_size > 0 ? _list_size * 2 : 1);
    _list[_list_count]._dev    = dev;
    _list[_list_count]._func = func;
    _list_count++;
  }

  /**
   * Send message LIFO.
   */
  bool  send(M &msg, bool earlyout = false)
  {
    _debug_counter++;
    bool res = false;
    for (unsigned i = _list_count; i-- && !(earlyout && res);)
      res |= _list[i]._func(_list[i]._dev, msg);
    return res;
  }

  /**
   * Send message in FIFO order
   */
  bool  send_fifo(M &msg)
  {
    _debug_counter++;
    bool res = false;
    for (unsigned i = 0; i < _list_count; i++)
      res |= _list[i]._func(_list[i]._dev, msg);
    return 0;
  }


  /**
   * Send message first hit round robin and return the number of the
   * next one that accepted the message.
   */
  bool  send_rr(M &msg, unsigned &start)
  {
    _debug_counter++;
    for (unsigned i = 0; i < _list_count; i++)
      if (_list[i]._func(_list[(i + start) % _list_count]._dev, msg)) {
	start = (i + start + 1) % _list_count;
	return true;
      }
    return false;
  }



  /**
   * Return the number of entries in the list.
   */
  unsigned count() { return _list_count; };

  /**
   * Debugging output.
   */
  void debug_dump()
  {
    Logging::printf("%s: Bus used %ld times.", __PRETTY_FUNCTION__, _debug_counter);
    for (unsigned i = 0; i < _list_count; i++)
      {
	Logging::printf("\n%2d:\t", i);
	_list[i]._dev->debug_dump();
      }
    Logging::printf("\n");
  }

  /** Default constructor. */
  DBus() {}
};
