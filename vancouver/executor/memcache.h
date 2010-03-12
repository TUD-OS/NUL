/**
 * Physical Memory Cache.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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
 * A cache for physical memory indexed by page number.
 */
class MemCache
{
protected:
  Motherboard &_mb;
private:
  enum {
    SIZE = 64,
    // Associativity, the minimum is 2 (movs).
    ASSOZ = 6,
    // Number of buffers, we need two for movs, push and similar instructions...
    BUFFERS = 6,
    // The maximum size of a buffer, the minmum is 16 (cmpxchg16b+instruction-reread).
    BUFFER_SIZE = 16,
  };

  // the hash function for the cache
  unsigned slot(unsigned phys) { phys = phys >> 12; return ((phys ^ (phys/SIZE)) % SIZE); }


public:
  /* universal access types */
  enum Type {
    TYPE_R   = 1u << 0,
    TYPE_W   = 1u << 1,
    TYPE_RMW = TYPE_R | TYPE_W,
    TYPE_U   = 1u << 2,
    TYPE_RES = 1u << 3,
    TYPE_X   = 1u << 4,
  };

  static Type user_access(MessageExecutor &msg, Type type)
  {
    if (msg.cpu->cpl() == 3) return Type(TYPE_U | type);
    return type;
  }

  struct CacheEntry
  {
    // the start address of this entry
    unsigned _phys1;
    unsigned _phys2;
    // 0 -> invalid, can be RAM up to 8k
    void *_ptr;
    // length of cache entry, this can be up to 8k long
    unsigned _len;
    // a pointer in a single linked list to an older entry in the set or ~0u at the end
    unsigned _older;
    bool is_valid(unsigned long phys1, unsigned long phys2, unsigned len)
    {
      return _ptr && phys1 == _phys1 && len == _len && phys2 == _phys2;
    }
  };

  bool debug;
private:
  struct {
    CacheEntry _values[ASSOZ];
    unsigned _newest;
  } _sets[SIZE];


  /**
   * Cache MMIO registers and pending writes to them.  The data is
   * sorted in two single linked lists. The usage list via
   * CacheEntry::_older and the dirty list.
   */
  struct Buffers : CacheEntry
  {
    unsigned _newer_write;
    unsigned char data[BUFFER_SIZE];
  } _buffers[BUFFERS];
  unsigned _newest_buffer;
  unsigned _oldest_write;
  unsigned _newest_write;

  /**
   * Invalidate the oldest dirty entry in the list.
   */
  void invalidate_dirty()
  {
    unsigned i = _oldest_write;
    assert(~i);

    _oldest_write = _buffers[i]._newer_write;
    if (_newest_write == i)
      {
	_newest_write = ~0;
	assert(_oldest_write == _newest_write);
      }
    _buffers[i]._newer_write = ~0;


    int sublen = _buffers[i]._len - 0x1000 + (_buffers[i]._phys1 & 0xfff);
    if (sublen < 0) sublen = 0;
    MessageMemWrite msg2(_buffers[i]._phys1, _buffers[i].data, _buffers[i]._len - sublen);
    _mb.bus_memwrite.send(msg2, true);

    if (sublen)
      {
	MessageMemWrite msg3(_buffers[i]._phys2, _buffers[i].data + _buffers[i]._len - sublen, sublen);
	_mb.bus_memwrite.send(msg3, true);
      }
  }


public:

/**
 * Move CacheEntries to the front of the usage list.
 */
#define return_move_to_front(set, newest)		\
  {							\
    if (~old)						\
      {							\
	set[old]._older =  set[entry]._older;		\
	set[entry]._older = newest;			\
	newest = entry;					\
      }							\
    return set + entry;					\
  }

/*
 * Search for an entry starting from the newest one.
 */
#define search_entry(set, newest)					\
  unsigned old = ~0;							\
  unsigned entry = newest;						\
  for (; ~set[entry]._older; old = entry, entry = set[entry]._older)	\
    if (set[entry].is_valid(phys1, phys2, len))				\
      return_move_to_front(set, newest);				\
  /* we have at least an assoziativity of two! */			\
  assert(~old);								\
  assert(~entry);							\


  /**
   * Get an entry from the cache or fetch one from memory.
   */
  CacheEntry *get(unsigned long phys1, unsigned long phys2, unsigned len, Type type)
  {
#if 1
    {
      unsigned s = slot(phys1);
      search_entry(_sets[s]._values, _sets[s]._newest);

      // try to get a direct memory reference
      void *new_ptr = 0;
      MessageMemAlloc msg1(&new_ptr, phys1 & ~0xffful, phys2 & ~0xffful);

      if (_mb.bus_memalloc.send(msg1, true) && new_ptr) {
	CacheEntry *res = _sets[s]._values + entry;
	res->_ptr = reinterpret_cast<char *>(new_ptr) + (phys1 & 0xfff);
	res->_len = len;
	res->_phys1 = phys1;
	res->_phys2 = phys2;
	return_move_to_front(_sets[s]._values, _sets[s]._newest);
      }
    }
#endif

    // we could not alloc the memory region directly from RAM, thus we use our own buffer instead.
    {
      assert(len <= BUFFER_SIZE);
      search_entry(_buffers, _newest_buffer);

      /**
       * Invalidate a dirty entry, as we entry points to the last used
       * buffer it must be the oldest write.
       */
      if (entry == _oldest_write) invalidate_dirty();
      assert(!~_buffers[entry]._newer_write);

      // init data as in the floating bus.
      memset(_buffers[entry].data, 0xff, sizeof(_buffers[entry].data));
      _buffers[entry]._ptr = &_buffers[entry].data;

      // put us on the write list
      if (type & TYPE_W)
	{
	  if (~_newest_write)
	      _buffers[_newest_write]._newer_write = entry;
	  else
	    _oldest_write = entry;
	  _newest_write = entry;
	}

      // do we have to read the data into the cache?
      if (type & TYPE_R)
	{
	  int sublen = len - 0x1000 + (phys1 & 0xfff);
	  if (sublen < 0)  sublen = 0;

	  MessageMemRead msg2(phys1, _buffers[entry].data, len - sublen);
	  _mb.bus_memread.send(msg2, true);

	  if (sublen)
	    {
	      MessageMemRead msg3(phys2, _buffers[entry].data +  len - sublen, sublen);
	      _mb.bus_memread.send(msg3, true);
	    }
	}
      // init entry
      _buffers[entry]._len   = len;
      _buffers[entry]._phys1 = phys1;
      _buffers[entry]._phys2 = phys2;
      return_move_to_front(_buffers, _newest_buffer);
    }
  }


  /**
   * Invalidate the cache, thus writeback the buffers.
   */
  void invalidate(bool writeback)
    {
      if (writeback)
	while (~_oldest_write) invalidate_dirty();
      else
	_oldest_write = _newest_write = ~0;
      for (unsigned i=0; i < BUFFERS; i++) { _buffers[i]._ptr = 0; _buffers[i]._newer_write = ~0; }
    }


 MemCache(Motherboard &mb) : _mb(mb), _sets()
  {
    assert(ASSOZ   >= 2);
    assert(BUFFERS >= 2);

    // init the cache sets
    for (unsigned j = 0; j < SIZE; j++)
      {
	_sets[j]._newest = ~0;
	for (unsigned i = 0; i < ASSOZ; i++)
	  {
	    _sets[j]._values[i]._older = _sets[j]._newest;
	    _sets[j]._newest = i;
	  }
      }

    // init the buffers
    _newest_buffer = ~0;
    for (unsigned i = 0; i < BUFFERS; i++)
      {
	_buffers[i]._older = _newest_buffer;
	_newest_buffer = i;
      }
    invalidate(false);
  }
};
