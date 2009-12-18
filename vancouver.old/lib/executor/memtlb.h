/**
 * Next TLB implementation.
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

#include "memcache.h"


/**
 * A TLB implementation relying on the cache.
 */
class MemTlb : public MemCache
{
public:
  static const unsigned PHYS_ADDR_SIZE = 40;

private:
  enum Features {
    FEATURE_PSE        = 1 << 0,
    FEATURE_PSE36      = 1 << 1,
    FEATURE_PAE        = 1 << 2,
    FEATURE_SMALL_PDPT = 1 << 3,
    FEATURE_LONG       = 1 << 4,
  };
  unsigned (*tlb_fill_func)(MemTlb *tlb, MessageExecutor &msg, unsigned long virt, unsigned type, long unsigned &phys);
  unsigned paging_mode;

#define AD_ASSIST(bits)							\
  if ((pte & (bits)) != (bits))						\
    {									\
      if (features & FEATURE_PAE)  Cpu::cmpxchg8b(entry->_ptr, pte, pte | bits); \
      else  Cpu::cmpxchg(entry->_ptr, pte, pte | bits);			\
    }
  

  template <unsigned features, typename PTE_TYPE>
  static unsigned tlb_fill(MemTlb *tlb, MessageExecutor &msg, unsigned long virt, unsigned type, long unsigned &phys)
  {
    PTE_TYPE pte;
    if (features & FEATURE_SMALL_PDPT) pte = VCPU(pdpt)[(virt >> 30) & 3]; else pte = READ(cr3);
    if (features & FEATURE_SMALL_PDPT && ~pte & 1) PF(virt, type & ~1);   
    if (~features & FEATURE_PAE || ~tlb->paging_mode & (1<<11)) type &= ~TYPE_X;
    unsigned r = type;
    unsigned l = features & FEATURE_LONG ? 4 : 2;
    bool is_sp;
    CacheEntry *entry = 0;
    do
      {
	if (entry) AD_ASSIST(0x20);
	if (features & FEATURE_PAE)  entry = tlb->get((pte & ~0xfff) | ((virt >> l* 9) & 0xff8ul), ~0xffful, 8, TYPE_R);
	else                         entry = tlb->get((pte & ~0xfff) | ((virt >> l*10) & 0xffcul), ~0xffful, 4, TYPE_R);
	pte = *reinterpret_cast<PTE_TYPE *>(entry->_ptr);
	if (~pte & 1) PF(virt, type & ~1);
	r &= pte | TYPE_X;
	l--;
	is_sp = l && l != 3 && pte & 0x80 && features & FEATURE_PSE;
	if (features & FEATURE_PAE && pte & (1ULL << 63)) r &= ~TYPE_X;

	// reserved bit checking
	bool reserved_bit = false;
	if (features & FEATURE_PSE36)  reserved_bit = is_sp && pte & (1 << 21);
	else if (features & FEATURE_PAE)
	  {
	    reserved_bit = (~tlb->paging_mode & (1<<11) && (pte & (1ULL << 63)))
	      || ((~features & FEATURE_LONG) && features & FEATURE_PAE && (static_cast<unsigned long long>(pte) >> 52) & 0xeff)
	      || (((static_cast<unsigned long long>(pte) & ~(1ULL << 63)) >> PHYS_ADDR_SIZE) || (is_sp && (pte >> 12) & ((1<<(l*9))-1)));
	  }
	if (reserved_bit)  PF(virt, type | 9);
      } while (l && !is_sp);
    
    // !wp: kernel write to read-only userpage? -> put the page as kernel read-write in the TLB
    if (~tlb->paging_mode & (1<<16) && type & TYPE_W && ~type & TYPE_U && ~r & TYPE_W && r & TYPE_U)
      r = (r | TYPE_W) & ~TYPE_U;

    // enough rights?
    if ((r & type) != type)  PF(virt, type | 1);
    
    // delete write flag, if we do not write and the dirty flag is not set
    if (~type & TYPE_W && ~pte & 1 << 6)  r &= ~TYPE_W;

    // update A+D bits
    AD_ASSIST((r & 3) << 5);

    unsigned size = ((features & FEATURE_PAE) ? 9 : 10) * l + 12;
    if (features & FEATURE_PSE36 && is_sp)
      phys = ((pte >> 22) | ((pte & 0x1fe000) >> 2));
    else
      phys = pte >> size;
    phys = (phys << size) | (virt & ((1 << size) - 1));    
    return msg.vcpu->fault;
  }
  
  int virt_to_phys(MessageExecutor &msg, unsigned long virt, Type type, long unsigned &phys)
  {
    if (tlb_fill_func) return tlb_fill_func(this, msg, virt, type, phys);
    phys = virt;
    return msg.vcpu->fault;
  }

  
protected:
  int init(MessageExecutor &msg) 
  {
    paging_mode = (READ(cr0) & 0x80010000) | READ(cr4) & 0x30 | VCPU(efer) & 0xc00;    

    // fetch pdpts in leagacy PAE mode
    if ((paging_mode & 0x80000420) == 0x80000020)
      {
	unsigned long long values[4];
	for (unsigned i=0; i < 4; i++)
	  {
	    values[i] = *reinterpret_cast<unsigned long long *>(get((READ(cr3) &~0x1f) + i*8, ~0xffful, 8, TYPE_R)->_ptr);
	    if ((values[i] & 0x1e6) || (values[i] >> (PHYS_ADDR_SIZE - 32)))  GP0;
	  }
	memcpy(msg.vcpu->pdpt, values, sizeof(msg.vcpu->pdpt));
      }

    // set paging mode
    tlb_fill_func = 0;
    if (paging_mode & 0x80000000)
      {
	tlb_fill_func = &tlb_fill<0, unsigned>;
	if (paging_mode & (1 << 4))  tlb_fill_func = &tlb_fill<FEATURE_PSE | FEATURE_PSE36, unsigned>;
	if (paging_mode & (1 << 5))
	  {
	    tlb_fill_func = &tlb_fill<FEATURE_PSE | FEATURE_PAE | FEATURE_SMALL_PDPT, unsigned long long>;    
	    if (paging_mode & (1 << 10))
	      tlb_fill_func = &tlb_fill<FEATURE_PSE | FEATURE_PAE | FEATURE_LONG, unsigned long long>;
	  }
      }
    return msg.vcpu->fault;
  }
  
  
  /**
   * Find a CacheEntry to a virtual memory access.
   */
  CacheEntry *find_virtual(MessageExecutor &msg, unsigned virt, unsigned len, Type type)
  {
    //if (len > 4)  Logging::printf("find_virtual %x, %x\n", virt, len);
    unsigned long phys1, phys2;
    if (!virt_to_phys(msg, virt, type, phys1) && !virt_to_phys(msg, virt + len - 1, type, phys2))
      {
	if (!((virt ^ (virt + len - 1)) & ~0xfff)) phys2 = ~0ul;
	return get(phys1, phys2 & ~0xfff, len, type);
      }
    return 0;
  }


  /**
   * Read the len instruction-bytes at the given address into a buffer.
   */
  int read_code(MessageExecutor &msg, unsigned long virt, unsigned len, void *buffer)
  {
    COUNTER_INC("read_code");
    
    assert(len < 16);
    CacheEntry *entry = find_virtual(msg, virt, len, user_access(msg, Type(TYPE_X | TYPE_R)));
    if (entry)
      {
	assert(len <= entry->_len);
	char *src = reinterpret_cast<char *>(entry->_ptr);
	#if 0
	if (len < entry->_len)
	  {
	    // the size needs to be a power of 2
	    assert(!(entry->_len   & (entry->_len - 1)));
	    assert(!(entry->_phys1 & (entry->_len - 1)));
	    src += virt & (entry->_len - 1);
	  }
	#endif
	//Logging::printf("%s %p -> %p count %x\n", __func__, src, buffer, len);
	memcpy(buffer, src, len);
      }
    return msg.vcpu->fault;
  }  


  int prepare_virtual(MessageExecutor &msg, unsigned virt, unsigned len, Type type, void *&ptr)
  {
    COUNTER_INC("prep_virtual");
    CacheEntry *entry = find_virtual(msg, virt, len, type);
    if (entry)
      {
	assert(len <= entry->_len);
	char *src = reinterpret_cast<char *>(entry->_ptr);
	#if 0
	if (len < entry->_len)
	  {
	    // the size needs to be a power of 2
	    Logging::panic("len %x vs %x phys %x\n", len, entry->_len, entry->_phys1);
	    assert(!(entry->_len   & (entry->_len - 1)));
	    assert(!(entry->_phys1 & (entry->_len - 1)));
	    src += virt & (entry->_len - 1);
	  }
	#endif
	ptr = src;
      }
    return msg.vcpu->fault;
  }



  MemTlb(Motherboard &mb) : MemCache(mb) {}
};
