/**
 * Bus infrastucture and generic Device class.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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

#include "driver/logging.h"
#include <cstdlib>
#include <cstring>

/**
 * The generic Device used in generic bus transactions.
 */
class Device
{
 public:
  virtual const char *debug_getname() = 0;
  virtual void debug_dump() {  Logging::printf("%s",debug_getname()); };
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
    unsigned _tag;
    Device *_dev;
    ReceiveFunction _func;
  };

  bool _debug;
  unsigned _list_count;
  unsigned _list_size;
  struct Entry *_list;


  void set_size(unsigned new_size)
  {
    void *n = malloc(new_size * sizeof(*_list));
    memcpy(n, _list, _list_count * sizeof(*_list));
    if (_list)  free(_list);
    _list = reinterpret_cast<struct Entry *>(n);
    _list_size = new_size;
  };
 public:

 DBus() : _debug(false), _list_count(0), _list_size(0), _list(0) {};

  void add(Device *dev, ReceiveFunction func, unsigned tag = ~0u)
  {
    if (_list_count >= _list_size)
      set_size(_list_size > 0 ? _list_size * 2 : 1);
    _list[_list_count]._dev    = dev;
    _list[_list_count]._func = func;
    _list[_list_count]._tag = tag;
    _list_count++;
  }

  /**
   * Send message LIFO.
   */
  bool  send(M &msg, bool earlyout = false, unsigned tag = ~0u)
  {
    bool res = false;
    if (_debug) Logging::printf("%s count %d tag %x\n", __PRETTY_FUNCTION__, _list_count, tag);	
    for (unsigned i = _list_count; i-- && !(earlyout && res);)
      {
	if (tag == ~0u || _list[i]._tag == tag)
	  res |= _list[i]._func(_list[i]._dev, msg);
      }
    return res;
  }

  /**
   * Send message in FIFO order 
   */
  bool  send_fifo(M &msg)
  {
    bool res = false;
    if (_debug) Logging::printf("%s count %d\n", __PRETTY_FUNCTION__, _list_count);	
    for (unsigned i = 0; i < _list_count; i++)
      res |= _list[i]._func(_list[i]._dev, msg);
    return 0;
  }



  /**
   * Return the number of entries in the list.
   */
  unsigned count() { return _list_count; };
  void debug() { _debug = true; };
  void debug_dump()
  {
    for (unsigned i = 0; i < _list_count; i++)
      {
	Logging::printf("\n%2d:   (%2d)\t", i, _list[i]._tag);
	_list[i]._dev->debug_dump();
      }
    Logging::printf("\n");
  }
};



/**
 * This template converts from static receive to member functions.
 */
template<typename Y>
class StaticReceiver : public Device
{
public:
  template<class M>
    static bool receive_static(Device *o, M& msg) { return static_cast<Y*>(o)->receive(msg); }
};



/**
 * Helper function.
 */
static inline bool in_range(unsigned long address, unsigned long base, unsigned long size) { return (base <= address) && (address <= base + size - 1); }
