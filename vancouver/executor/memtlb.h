/** @file
 * Next TLB implementation.
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
#include "nul/config.h"
#include "memcache.h"


/**
 * A TLB implementation relying on the cache.
 */
class MemTlb : public MemCache
{
protected:
  CpuState *_cpu;

private:
  // pdpt cache for 32-bit PAE
  unsigned long long _pdpt[4];
  unsigned long _msr_efer;
  unsigned _paging_mode;

  enum Features {
    FEATURE_PSE        = 1 << 0,
    FEATURE_PSE36      = 1 << 1,
    FEATURE_PAE        = 1 << 2,
    FEATURE_SMALL_PDPT = 1 << 3,
    FEATURE_LONG       = 1 << 4,
  };
  unsigned (*tlb_fill_func)(MemTlb *tlb, unsigned long virt, unsigned type, long unsigned &phys);

#define AD_ASSIST(bits)							\
  if ((pte & (bits)) != (bits))						\
    {									\
    if (features & FEATURE_PAE) {					\
      if (Cpu::cmpxchg8b(entry->_ptr, pte, pte | bits) != pte) RETRY;	\
    }									\
    else								\
      if (Cpu::cmpxchg4b(entry->_ptr, pte, pte | bits) != pte) RETRY;	\
    }

  template <unsigned features, typename PTE_TYPE>
    static unsigned tlb_fill(MemTlb *tlb, unsigned long virt, unsigned type, long unsigned &phys)
  {  return tlb->tlb_fill2<features, PTE_TYPE>(virt, type, phys); }


  template <unsigned features, typename PTE_TYPE>
    unsigned tlb_fill2(unsigned long virt, unsigned type, long unsigned &phys)
  {
    PTE_TYPE pte;
    if (features & FEATURE_SMALL_PDPT) pte = _pdpt[(virt >> 30) & 3]; else pte = READ(cr3);
    if (features & FEATURE_SMALL_PDPT && ~pte & 1) PF(virt, type & ~1);
    if (~features & FEATURE_PAE || ~_paging_mode & (1<<11)) type &= ~TYPE_X;
    unsigned rights = TYPE_R | TYPE_W | TYPE_U | TYPE_X;
    unsigned l = features & FEATURE_LONG ? 4 : 2;
    bool is_sp;
    CacheEntry *entry = 0;
    do
      {
	if (entry) AD_ASSIST(0x20);
	if (features & FEATURE_PAE)  entry = get((pte & ~0xfff) | ((virt >> l* 9) & 0xff8ul), ~0xffful, 8, TYPE_R);
	else                         entry = get((pte & ~0xfff) | ((virt >> l*10) & 0xffcul), ~0xffful, 4, TYPE_R);
	pte = *reinterpret_cast<PTE_TYPE *>(entry->_ptr);
	if (~pte & 1)  PF(virt, type & ~1);
	rights &= pte | TYPE_X;
	l--;
	is_sp = l && l != 3 && pte & 0x80 && features & FEATURE_PSE;
	if (features & FEATURE_PAE && pte & (1ULL << 63)) rights &= ~TYPE_X;

	// reserved bit checking
	bool reserved_bit = false;
	if (features & FEATURE_PSE36)  reserved_bit = is_sp && pte & (1 << 21);
	else if (features & FEATURE_PAE)
	  {
	    reserved_bit = (~_paging_mode & (1<<11) && (pte & (1ULL << 63)))
	      || ((~features & FEATURE_LONG) && features & FEATURE_PAE && (static_cast<unsigned long long>(pte) >> 52) & 0xeff)
	      || (((static_cast<unsigned long long>(pte) & ~(1ULL << 63)) >> Config::PHYS_ADDR_SIZE) || (is_sp && (pte >> 12) & ((1<<(l*9))-1)));
	  }
	if (reserved_bit)  PF(virt, type | 9);
      } while (l && !is_sp);

    // !wp: kernel write to read-only userpage? -> put the page as kernel read-write in the TLB
    if (~_paging_mode & (1<<16) && type & TYPE_W && ~type & TYPE_U && ~rights & TYPE_W && rights & TYPE_U)
      rights = (rights | TYPE_W) & ~TYPE_U;

    // enough rights?
    if ((rights & type) != type)  PF(virt, type | 1);

    // delete write flag, if we do not write and the dirty flag is not set
    if (~type & TYPE_W && ~pte & 1 << 6)  rights &= ~TYPE_W;

    // update A+D bits
    AD_ASSIST((rights & 3) << 5);

    unsigned size = ((features & FEATURE_PAE) ? 9 : 10) * l + 12;
    if (features & FEATURE_PSE36 && is_sp)
      phys = ((pte >> 22) | ((pte & 0x1fe000) >> 2));
    else
      phys = pte >> size;
    phys = (phys << size) | (virt & ((1 << size) - 1));
    return _fault;
  }

  int virt_to_phys(unsigned long virt, Type type, long unsigned &phys) {

    if (tlb_fill_func) return tlb_fill_func(this, virt, type, phys);
    phys = virt;
    return _fault;
  }

  /**
   * Find a CacheEntry to a virtual memory access.
   */
  CacheEntry *find_virtual(unsigned virt, unsigned len, Type type) {

    unsigned long phys1, phys2;
    if (!virt_to_phys(virt, type, phys1)) {
      if (!((virt ^ (virt + len - 1)) & ~0xfff)) phys2 = ~0ul;
      else
	if (virt_to_phys(virt + len - 1, type, phys2)) return 0;
      return get(phys1, phys2 & ~0xffful, len, type);
    }
    return 0;
  }

protected:
  Type user_access(Type type) {
    if (_cpu->cpl() == 3) return Type(TYPE_U | type);
    return type;
  }


  int init() {

    _paging_mode = (READ(cr0) & 0x80010000) | READ(cr4) & 0x30 | _msr_efer & 0xc00;

    // fetch pdpts in leagacy PAE mode
    if ((_paging_mode & 0x80000420) == 0x80000020)
      {
	unsigned long long values[4];
	for (unsigned i=0; i < 4; i++)
	  {
	    values[i] = *reinterpret_cast<unsigned long long *>(get((READ(cr3) &~0x1f) + i*8, ~0xffful, 8, TYPE_R)->_ptr);
	    if ((values[i] & 0x1e6) || (values[i] >> Config::PHYS_ADDR_SIZE))  GP0;
	  }
	memcpy(_pdpt, values, sizeof(_pdpt));
      }

    // set paging mode
    tlb_fill_func = 0;
    if (_paging_mode & 0x80000000)
      {
	tlb_fill_func = &tlb_fill<0, unsigned>;
	if (_paging_mode & (1 << 4))  tlb_fill_func = &tlb_fill<FEATURE_PSE | FEATURE_PSE36, unsigned>;
	if (_paging_mode & (1 << 5))
	  {
	    tlb_fill_func = &tlb_fill<FEATURE_PSE | FEATURE_PAE | FEATURE_SMALL_PDPT, unsigned long long>;
	    if (_paging_mode & (1 << 10))
	      tlb_fill_func = &tlb_fill<FEATURE_PSE | FEATURE_PAE | FEATURE_LONG, unsigned long long>;
	  }
      }
    return _fault;
  }


  /**
   * Read the len instruction-bytes at the given address into a buffer.
   */
  int read_code(unsigned long virt, unsigned len, void *buffer)
  {
    assert(len < 16);
    CacheEntry *entry = find_virtual(virt & ~3, (len + (virt & 3) + 3) & ~3ul, user_access(Type(TYPE_X | TYPE_R)));
    if (entry) {
      assert(len <= entry->_len);
      memcpy(buffer, entry->_ptr + (virt & 3), len);
    } else
      // fix CR2 value as we rounded down
      if (_fault == 0x80000b0e && _cpu->cr2 < virt)
	_cpu->cr2 = virt;
    return _fault;
  }


  int prepare_virtual(unsigned virt, unsigned len, Type type, void *&ptr)
  {
    bool round = (virt | len) & 3;
    CacheEntry *entry = find_virtual(virt & ~3ul, (len + (virt & 3) + 3) & ~3ul, round ? Type(type | TYPE_R) : type);
    if (entry) {
      assert(len <= entry->_len);
      ptr = entry->_ptr + (virt & 3);
    }
    return _fault;
  }


  MemTlb(DBus<MessageMem> &mem, DBus<MessageMemRegion> &memregion) : MemCache(mem, memregion) {}
};
