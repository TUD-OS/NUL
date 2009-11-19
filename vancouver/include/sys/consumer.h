/*
 * Generic NOVA producer/consumer code based on shared memory and
 * semaphores.
 *
 * Copyright (C) 2008-2009, Bernhard Kauer <kauer@tudos.org>
 *
 * This file is part of Vancouver.nova.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
#pragma once
#include <nova.h>
#include "sys/semaphore.h"


/**
 * Consumer with fixed items.
 */
template <typename T, unsigned SIZE>
class Consumer
{
public:
  Semaphore _sem;
  long      _count;
  unsigned  _rpos;
  unsigned  _wpos;
  T         _buffer[SIZE];
  Nova::Cap_idx sm() { return _sem.sm(); }

  T * get_buffer() {
    _sem.down();
    return _buffer + _rpos;
  }

  void free_buffer()  { _rpos = (_rpos + 1) % SIZE; }


  Consumer(Nova::Cap_idx cap_sm) : _sem(Semaphore(&_count, cap_sm)),  _count(0), _rpos(0), _wpos(0) 
  { 
    unsigned res;
    if ((res = Nova::create_sm(cap_sm, 0)))
      Logging::panic("create Consumer failed with %x\n", res);
    Logging::printf("create Consumer ok with %x nq %x\n", res, cap_sm);
  }
};



/**
 * Producer with fixed items.
 */
template <typename T, unsigned SIZE>
class Producer
{
protected:
  Consumer<T, SIZE> *_consumer;
  Semaphore          _sem;
  bool               _dropping;

public:
  /**
   * Put something in the buffer. Please note that this function is
   * not locked, thus only a single producer should do the access at
   * the very same time.
   */
  bool produce(T &value)
  {
    if (!_consumer || ((_consumer->_wpos + 1) % SIZE == _consumer->_rpos)) 
      {
	//if (!_dropping) Logging::printf("%s dropping write due to overflow\n", __PRETTY_FUNCTION__);
	_dropping = true;
	return false;
      }
    _dropping = false;
    _consumer->_buffer[_consumer->_wpos] = value;
    _consumer->_wpos = (_consumer->_wpos + 1) % SIZE;
    _sem.up();
    return true;
  }

  Producer(Consumer<T, SIZE> *consumer = 0, unsigned nq = 0) : _consumer(consumer), _sem(&consumer->_count, nq) {};
};


/**
 * Packet consumer that supports variable sized packets.
 */
template <unsigned SIZE>
class PacketConsumer : public Consumer<unsigned, SIZE>
{
  typedef Consumer<unsigned, SIZE> Parent;
public:
  /**
   * Get a pointer to a the buffer and return the length of the buffer.
   */
  unsigned get_buffer(char *&buffer)
  {
    unsigned *len = Parent::get_buffer();
    if (*len == ~0u)
      {
	Parent::_rpos = 0;
	len = Parent::_buffer + Parent::_rpos;
      }
    assert(*len < sizeof(unsigned) * SIZE);
    buffer =  reinterpret_cast<char *>(Parent::_buffer + Parent::_rpos + 1);
    return *len;
  }

  void free_buffer()
  {
    Parent::_rpos = (Parent::_rpos + (Parent::_buffer[Parent::_rpos] + 2*sizeof(unsigned) - 1)/sizeof(unsigned)) % SIZE;
  }

  PacketConsumer(unsigned cap_sm) : Parent(cap_sm) {}
};


/**
 * Packet producer that supports variable sized packets.
 */
template <unsigned SIZE>
class PacketProducer : public Producer<unsigned, SIZE>
{
  typedef Producer<unsigned, SIZE> Parent;
public:
  PacketProducer(PacketConsumer<SIZE> *consumer=0, unsigned cap_nq=0) : Producer<unsigned, SIZE>(consumer, cap_nq) {}

  /**
   * Put something in the buffer. Please note that this function is
   * not locked, thus only a single producer should do the access at
   * the very same time.
   */
 bool produce(const unsigned char *buf, unsigned len)
  {
    if (!Parent::_consumer || !len) return false;
    unsigned right = SIZE - Parent::_consumer->_wpos;
    unsigned left = Parent::_consumer->_rpos;
    unsigned needed = (len + 2*sizeof(unsigned) - 1) / sizeof(unsigned);
    if (left > Parent::_consumer->_wpos)
      {
	right = left - Parent::_consumer->_wpos;
	left = 0;
      }

    if ((needed >= right) && (needed >= left))
      {
	//if (!Parent::_dropping) Logging::printf("%s dropping write due to overflow %x<%x>%x \n", __PRETTY_FUNCTION__, left, needed, right);
	Parent::_dropping = true;
	return false;
      }
    //Logging::printf("%s %p %x - pos %x,%x\n", __PRETTY_FUNCTION__, buf, len, _consumer->_wpos, _consumer->_rpos);
    Parent::_dropping = false;
    
    unsigned ofs = Parent::_consumer->_wpos; 
    if (right < needed) {
      if (right !=0) Parent::_consumer->_buffer[ofs] = ~0u;
      ofs = 0;
    }
    Parent::_consumer->_buffer[ofs] = len;
    memcpy(Parent::_consumer->_buffer + ofs + 1, buf, len);
    Parent::_consumer->_wpos = ofs + needed;
    Parent::_sem.up();
    return true;
  }
};
