/*
 * Common code for NOVA programs.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of Vancouver.
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

#include <sys/utcb.h>

/**
 * A simple program that allows to get the UTCB pointer from the stack.
 */
struct BaseProgram {
  /* XXX Don't forget to change the initial stack in nul/program.h at
     __start */
  static const unsigned stack_size_shift = 12;
  static const unsigned stack_size = (1U << stack_size_shift);

  /**
   * Get the UTCB pointer from the top of the stack.
   */
  static Utcb *myutcb() { unsigned long esp; asm volatile ("mov %%esp, %0" : "=r"(esp));
    return *reinterpret_cast<Utcb **>( ((esp & ~(stack_size-1)) + stack_size - sizeof(void *)));
  };

  /**
   * add mappings to a UTCB.
   */
  static unsigned long add_mappings(Utcb *utcb, bool exception, unsigned long addr, unsigned long size, unsigned long hotspot, unsigned rights)
  {
    while (size > 0)
      {
	unsigned minshift = Cpu::minshift(addr | hotspot, size);
	assert(minshift >= Utcb::MINSHIFT);
	unsigned *item = (exception ? utcb->items : (utcb->msg + utcb->head.mtr.untyped())) + utcb->head.mtr.typed() * 2;
	if (reinterpret_cast<Utcb *>(item) >= utcb+1 || utcb->head.mtr.typed() >= 255) return size;
	item[0] = hotspot;
	item[1] = addr | ((minshift-Utcb::MINSHIFT) << 7) | rights;
	utcb->head.mtr = Mtd(utcb->head.mtr.untyped(), utcb->head.mtr.typed() + 1);

	unsigned long mapsize = 1 << minshift;
	size    -= mapsize;
	addr    += mapsize;
	hotspot += mapsize;
      }
    return size;
  };

  /**
   * Revoke all memory for a given virtual region.
   */
  static void revoke_all_mem(void *address, unsigned long size, unsigned rights, bool myself) {

    unsigned long page = reinterpret_cast<unsigned long>(address);
    size += page & 0xfff;
    page >>= 12;
    size = (size + 0xfff) >> 12;
    while (size) {
      unsigned order = Cpu::minshift(page, size);
      check0(revoke(Crd(page, order, rights | 1), myself));
      size -= 1 << order;
      page += 1 << order;
    }
  }



  /**
   * Request the mapping for a memory area.
   */
  static Crd request_mapping(char *start, unsigned long size, unsigned long hotspot) {
    assert(hotspot < size);

    Crd res = nova_lookup(start + hotspot);
    Logging::printf("request mapping %p+%lx s %lx -> %x\n", start, hotspot, size, res.value());
    if (!res.attr()) {
      // XXX request the mapping from sigma0 if nothing found
      asm volatile("lock orl $0, (%0)" : : "r"(start + hotspot) : "memory");
      res = nova_lookup(start + hotspot);
      // if this call fails, order==0 and the loop terminates
    }

    // restrict it to a region that fits into [start, start+size)
    // XXX can we avoid the loop?
    for (int i = res.order(); i >= 0; i--) {
      Crd x = Crd(((reinterpret_cast<unsigned long>(start) + hotspot) & ~((1 << (i+12)) - 1)) >> 12, i, DESC_MEM_ALL);
      //Logging::printf("%d %x vs %p and %x vs %lx\n", i, x.base(), start, x.base() + x.size(), reinterpret_cast<unsigned long>(start)+size);
      if ((x.base() >= reinterpret_cast<unsigned long>(start)) && (x.base() + x.size()) <=  (reinterpret_cast<unsigned long>(start)+size))
	return x;
    }
    Logging::panic("XXX nothing found %p-%lx-%lx\n", start, hotspot, size);
  }
};


/**
 * A template to simplify saving the utcb.
 */
template <unsigned words>
class TemporarySave
{
  void *_ptr;
  unsigned long _data[words];

 public:
 TemporarySave(void *ptr) : _ptr(ptr) { memcpy(_data, _ptr, words*sizeof(unsigned long)); }
  ~TemporarySave() { memcpy(_ptr, _data, words*sizeof(unsigned long)); }
};
