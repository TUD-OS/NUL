/** @file
 * Physical Memory Cache.
 *
 * Copyright (C) 2009-2010, Bernhard Kauer <bk@vmmon.org>
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

#define READ(NAME) ({ _mtr_read |= RMTR_##NAME; _cpu->NAME; })
#define WRITE(NAME) ({							\
      _mtr_out |= RMTR_##NAME;						\
      _cpu->NAME;							\
})
#define FAULT(NAME, VALUE) { NAME->_debug_fault_line = __LINE__;  NAME->_fault = VALUE; }
#define UNIMPLEMENTED(NAME) { return (FAULT(NAME, FAULT_UNIMPLEMENTED)); }
#define RETRY         { return (FAULT(this, FAULT_RETRY)); }
#define EXCEPTION0(NAME, NR) { NAME->_error_code = 0; FAULT(NAME, 0x80000300 | NR); }
#define EXCEPTION(NAME, NR, ERROR) { NAME->_error_code = ERROR; FAULT(NAME, 0x80000b00 | NR); }
#define DE0(X) { EXCEPTION0(X, 0x0); }
#define UD0   { EXCEPTION0(this, 0x6); return _fault; }
#define NP(X) { EXCEPTION(this, 0xb, X); return _fault; }
#define SS(X) { EXCEPTION(this, 0xc, X); return _fault; }
#define SS0   { EXCEPTION(this, 0xc, 0); return _fault; }
#define GP(X) { EXCEPTION(this, 0xd, X); return _fault; }
#define GP0   { EXCEPTION(this, 0xd, 0); return _fault; }
#define PF(ADDR, ERR) { _cpu->cr2 = ADDR; _mtr_out |= MTD_CR; EXCEPTION(this, 0xe, ERR); return _fault; }


/**
 * A cache for physical memory indexed by page number.
 */
class MemCache
{
protected:
  DBus<MessageMem>       &_mem;
  DBus<MessageMemRegion> &_memregion;
  unsigned  _fault;
  unsigned  _error_code;
  unsigned  _debug_fault_line;
  unsigned  _mtr_in;
  unsigned  _mtr_read;
  unsigned  _mtr_out;
private:
  enum {
    SIZE = 64,
    // Associativity, the minimum is 2 (movs).
    ASSOZ = 6,
    // Number of buffers, we need two for movs, push and similar instructions...
    BUFFERS = 6,
    // The maximum size of a buffer, the minmum is 16+dword (cmpxchg16b+instruction-reread).
    BUFFER_SIZE = 16 + 4
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
    TYPE_X   = 1u << 4
  };


  struct CacheEntry
  {
    // the start address of this entry
    unsigned _phys1;
    unsigned _phys2;
    // 0 -> invalid, can be RAM up to 8k
    char *_ptr;
    // length of cache entry, this can be up to 8k long
    unsigned _len;
    // a pointer in a single linked list to an older entry in the set or ~0u at the end
    unsigned _older;
    bool is_valid(unsigned long phys1, unsigned long phys2, unsigned len)
    {
      if (!_ptr) return false;
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
    char data[BUFFER_SIZE];
  } _buffers[BUFFERS];
  unsigned _newest_buffer;
  unsigned _oldest_write;
  unsigned _newest_write;


  void buffer_io(bool read, unsigned index) {
    assert(!(_buffers[index]._len & 3));
    assert(!(_buffers[index]._phys1 & 3));

    unsigned long address = _buffers[index]._phys1;
    for (unsigned i=0; i < _buffers[index]._len; i += 4) {
      MessageMem msg2(read, address, reinterpret_cast<unsigned *>(_buffers[index].data + i));
      _mem.send(msg2, true);
      if ((address & 0xfff) != 0xffc)
	address += 4;
      else
	address = _buffers[index]._phys2;
    }
  }


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

    buffer_io(false, i);
  }


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


public:

  /**
   * Get an entry from the cache or fetch one from memory.
   */
  CacheEntry *get(unsigned long phys1, unsigned long phys2, unsigned len, Type type)
  {
    assert(!(phys1 & 3));
    assert(!(len & 3));

    // XXX simplify it by relying on memory ranges
    {
      unsigned s = slot(phys1);
      search_entry(_sets[s]._values, _sets[s]._newest);

      /**
       * What should we do if two different pages are referenced?
       *
       * We could fallback to dword mode but there is this strange
       * corner case where somebody does an locked operation crossing
       * two non-adjunct pages, where we have to map the two pages
       * into an 8k region and unmap them later on....
       */
      bool supported = true;
      if (phys2 != ~0xffful && (((phys1 >> 12) + 1) != (phys2 >> 12))) {
	Logging::printf("joining two non-adjunct pages %lx,%lx is not supported\n", phys1 >> 12, phys2 >> 12);
	supported = false;
      }

      // try to get a direct memory reference
      MessageMemRegion msg1(phys1 >> 12);
      if (supported && _memregion.send(msg1, true) && msg1.ptr && ((phys1 + len) <= ((msg1.start_page + msg1.count) << 12))) {
	CacheEntry *res = _sets[s]._values + entry;
	res->_ptr = msg1.ptr + (phys1 - (msg1.start_page << 12));
	res->_len = len;
	res->_phys1 = phys1;
	res->_phys2 = phys2;
	return_move_to_front(_sets[s]._values, _sets[s]._newest);
      }
    }

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
      _buffers[entry]._ptr = _buffers[entry].data;

      // put us on the write list
      if (type & TYPE_W)
	{
	  if (~_newest_write)
	      _buffers[_newest_write]._newer_write = entry;
	  else
	    _oldest_write = entry;
	  _newest_write = entry;
	}

      // init entry
      _buffers[entry]._len   = len;
      _buffers[entry]._phys1 = phys1;
      _buffers[entry]._phys2 = phys2;

      // do we have to read the data into the cache?
      if (type & TYPE_R) buffer_io(true, entry);

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


  MemCache(DBus<MessageMem> &mem, DBus<MessageMemRegion> &memregion) : _mem(mem), _memregion(memregion), _sets()
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
