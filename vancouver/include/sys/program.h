/*
 * Common code for NOVA programs. 
 *
 * Copyright (C) 2008, Bernhard Kauer <kauer@tudos.org>
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
#include <cstring>
#include <cstdlib>
#include "vmm/cpu.h"
#include "driver/logging.h"
#include "driver/vprintf.h"
#include "sys/hip.h"
#include "sys/syscalls.h"
#include "sys/region.h"


#define check(X, ...) ({ unsigned __res; if ((__res = X)) { Logging::printf("%s() line %d: '" #X "' error = %x", __func__, __LINE__, __res); Logging::printf(" " __VA_ARGS__); Logging::printf("\n"); return 0xbead; }})


/**
 * Define the functions needed by asm.s.
 * Unfortunately this can not be a template.
 */
#define ASMFUNCS(X)				\
  extern "C" void start(Hip *hip)		\
  {						\
    (new X())->run(hip);			\
  }						\
  extern "C" void __exit(unsigned long status)	\
  {						\
    X::exit(status);				\
    while (1)					\
      asm volatile ("ud2");			\
  }




/**
 * Contains common code for nova programms.
 */
class NovaProgram
{

  enum {
    VIRT_START       = 0x1000,
    UTCB_PAD         = 0x1000,
  };
 protected:

  Hip *     _hip;
  Utcb *    _boot_utcb;
  unsigned  _cap_free;
  unsigned  _cap_block;

  // memory map
  RegionList<512> _free_virt;
  RegionList<512> _free_phys;
  RegionList<512> _virt_phys;


  /**
   * Get the UTCB pointer from the top of the stack. This is a hack, as long we do not have a myself systemcall!
   */
  static Utcb *myutcb() { unsigned long esp; asm volatile ("mov %%esp, %0" : "=r"(esp)); return *reinterpret_cast<Utcb **>(((esp & ~0x3) | 0xffc)); };
  
  /**
   * Alloc a region of virtual memory to put an EC into
   */
  Utcb * alloc_utcb() { return reinterpret_cast<Utcb *>(_free_virt.alloc(2*UTCB_PAD + sizeof(Utcb), 12) + UTCB_PAD); }

  /**
   * Create an ec and setup the stack.
   */
  unsigned  __attribute__((noinline))  create_ec_helper(unsigned tls, Utcb **utcb_out=0, bool worker = false, unsigned excbase = 0, unsigned cpunr=~0)
  {
    const unsigned stack_size = 0x1000;
    const unsigned STACK_FRAME = 2;

    Utcb *utcb = alloc_utcb();
    void **stack = reinterpret_cast<void **>(memalign(0x1000, stack_size));
    cpunr = (cpunr == ~0u) ? Cpu::cpunr() : cpunr;
    stack[stack_size/sizeof(void *) - 1] = utcb;
    stack[stack_size/sizeof(void *) - 2] = reinterpret_cast<void *>(idc_reply_and_wait_fast);
    Logging::printf("\t\tcreate ec[%x,%x] stack %p utcb %p at %p = %p tls %x\n", 
		    cpunr, _cap_free, stack, utcb, stack + stack_size/sizeof(void *) -  STACK_FRAME, stack[stack_size/sizeof(void *) -  STACK_FRAME], tls);
    check(create_ec(_cap_free, utcb,  stack + stack_size/sizeof(void *) -  STACK_FRAME, cpunr, excbase, worker));
    utcb->head.tls = tls;
    if (utcb_out)
      *utcb_out = utcb;
    return _cap_free++;
  }


  /**
   * Initialize ourself.
   */
  unsigned __attribute__((noinline)) init(Hip *hip)
  {
    check(hip->calc_checksum());
    _hip = hip;
    _boot_utcb = reinterpret_cast<Utcb *>(hip) - 1;
    _cap_free = hip->cfg_exc + hip->cfg_gsi;
    create_sm(_cap_block = _cap_free++);

    // prepopulate phys+virt memory
    extern char __image_start, __image_end;
    _free_virt.add(Region(VIRT_START, reinterpret_cast<unsigned long>(_boot_utcb) - VIRT_START));
    _free_virt.del(Region(reinterpret_cast<unsigned long>(&__image_start), &__image_end - &__image_start));
    _virt_phys.add(Region(reinterpret_cast<unsigned long>(&__image_start), &__image_end - &__image_start, hip->get_mod(0)->addr));
    return 0;
  };

  /**
   * Block ourself.
   */
  void __attribute__((noreturn)) block_forever() { while (1) semdown(_cap_block); };
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
