/*
 * Common code for NOVA programs.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
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
#include "service/string.h"
#include "service/cpu.h"
#include "service/logging.h"
#include "sys/hip.h"
#include "sys/syscalls.h"
#include "region.h"
#include "baseprogram.h"


/**
 * Define the functions needed by asm.s.
 * Unfortunately this can not be a template.
 */
#define ASMFUNCS(X, Y)							\
  extern "C" void start(Hip *hip, Utcb *utcb) __attribute__((regparm(2))); \
  void start(Hip *hip, Utcb *utcb)					\
  {									\
    static X x;								\
    x.run(utcb, hip);							\
  }									\
  void do_exit(const char *msg)						\
  {									\
    Y::exit(msg);							\
    while (1)								\
      asm volatile ("ud2a" : : "a"(msg));				\
  }




/**
 * Contains common code for nova programms.
 */
class NovaProgram : public BaseProgram
{

  enum {
    VIRT_START       = 0x1000,
    UTCB_PAD         = 0x1000,
  };
 protected:

  Hip *     _hip;
  unsigned  _cap_free;
  unsigned  _cap_block;

  // memory map
  RegionList<512> _free_virt;
  RegionList<512> _free_phys;
  RegionList<512> _virt_phys;


  /**
   * Alloc a region of virtual memory to put an EC into
   */
  Utcb * alloc_utcb() { return reinterpret_cast<Utcb *>(_free_virt.alloc(2*UTCB_PAD + sizeof(Utcb), 12) + UTCB_PAD); }

  /**
   * Create an ec and setup the stack.
   */
  unsigned  create_ec_helper(unsigned tls, Utcb **utcb_out=0, bool worker = false, unsigned excbase = 0, unsigned cpunr=~0)
  {
    const unsigned STACK_FRAME = 2;

    Utcb *utcb = alloc_utcb();
    void **stack = new(stack_size) void *[stack_size / sizeof(void *)];
    cpunr = (cpunr == ~0u) ? Cpu::cpunr() : cpunr;
    stack[stack_size/sizeof(void *) - 1] = utcb;
    stack[stack_size/sizeof(void *) - 2] = reinterpret_cast<void *>(idc_reply_and_wait_fast);
    Logging::printf("\t\tcreate ec[%x,%x] stack %p utcb %p at %p = %p tls %x\n",
		    cpunr, _cap_free, stack, utcb, stack + stack_size/sizeof(void *) -  STACK_FRAME, stack[stack_size/sizeof(void *) -  STACK_FRAME], tls);
    check1(0, create_ec(_cap_free, utcb,  stack + stack_size/sizeof(void *) -  STACK_FRAME, cpunr, excbase, worker));
    utcb->head.tls = tls;
    if (utcb_out)
      *utcb_out = utcb;
    return _cap_free++;
  }


  /**
   * Initialize ourself.
   */
  unsigned init(Hip *hip)
  {
    check1(1, hip->calc_checksum());
    _hip = hip;
    _cap_free = hip->cfg_exc + hip->cfg_gsi + 3;
    create_sm(_cap_block = _cap_free++);

    // prepopulate phys+virt memory
    extern char __image_start, __image_end;
    // add all memory, this does not include the boot_utcb, the HIP and the kernel!
    _free_virt.add(Region(VIRT_START, reinterpret_cast<unsigned long>(reinterpret_cast<Utcb *>(hip) - 1) - VIRT_START));
    _free_virt.del(Region(reinterpret_cast<unsigned long>(&__image_start), &__image_end - &__image_start));
    _virt_phys.add(Region(reinterpret_cast<unsigned long>(&__image_start), &__image_end - &__image_start, hip->get_mod(0)->addr));
    return 0;
  };


  /**
   * Init the memory map from the hip.
   */
  void init_mem() {

    for (int i=0; i < (_hip->length - _hip->mem_offs) / _hip->mem_size; i++) {
	Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(_hip) + _hip->mem_offs) + i;
	if (hmem->type == 1)  _free_phys.add(Region(hmem->addr, hmem->size, hmem->addr));
    }

    for (int i=0; i < (_hip->length - _hip->mem_offs) / _hip->mem_size; i++) {
      Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(_hip) + _hip->mem_offs) + i;
      if (hmem->type !=  1) _free_phys.del(Region(hmem->addr, (hmem->size+ 0xfff) & ~0xffful));
      // make sure to remove the cmdline
      if (hmem->type == -2 && hmem->aux)  {
	_free_phys.del(Region(hmem->aux, (strlen(reinterpret_cast<char *>(hmem->aux)) + 0xfff) & ~0xffful));
      }
    }
    // remove our binary from the free phys region.
    extern char __image_start, __image_end;
    _free_phys.del(Region(reinterpret_cast<unsigned long>(&__image_start), &__image_end - &__image_start));
  }


  /**
   * Block ourself.
   */
  void __attribute__((noreturn)) block_forever() { while (1) semdown(_cap_block); };

public:
  /**
   * Default exit function.
   */
  static void exit(const char *msg)
  {
    Logging::printf("%s() - %s\n", __func__, msg);
  }
};
