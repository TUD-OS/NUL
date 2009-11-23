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
#include <string.h>
#include <cstdlib>
#include "driver/logging.h"
#include "driver/vprintf.h"
#include <nova.h>
#include "sys/region.h"


#define check(X, ...) ({ unsigned __res; if ((__res = X)) { Logging::printf("%s() line %d: '" #X "' error = %x", __func__, __LINE__, __res); Logging::printf(" " __VA_ARGS__); Logging::printf("\n"); return 0xbead; }})


/**
 * Define the functions needed by asm.s.
 * Unfortunately this can not be a template.
 */
#define ASMFUNCS(X)				\
  extern "C" void start(Nova::Hip *hip)		\
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

  typedef ::Nova::Utcb Utcb;
  typedef ::Nova::Hip Hip;
  typedef ::Nova::Cap_idx Cap_idx;

  Hip  *_hip;
  Utcb *_boot_utcb;
  Cap_idx  _cap_free;
  Cap_idx  _cap_block;

  // memory map
  RegionList<512> _free_virt;
  RegionList<512> _free_phys;
  RegionList<512> _virt_phys;


  /**
   * Get the UTCB pointer from the top of the stack. This is a hack, as long we do not have a myself systemcall!
   */
  static Utcb *myutcb() { unsigned long esp; asm volatile ("mov %%esp, %0" : "=r"(esp)); return *reinterpret_cast<Utcb **>(((esp & ~0x3) | 0x3ffc)); };


  /**
   * Create an ec and setup the stack.
   */
  unsigned create_ec_helper(unsigned tls, Utcb **utcb_out=0)
  {
    const unsigned stack_size = 0x4000;
    const unsigned STACK_FRAME = 2;

    Utcb *utcb = reinterpret_cast<Utcb *>(_free_virt.alloc(2*UTCB_PAD + sizeof(Utcb), 12) + UTCB_PAD);
    void **stack = reinterpret_cast<void **>(memalign(0x4000, stack_size));
    
    stack[stack_size/sizeof(void *) - 1] = utcb;
    stack[stack_size/sizeof(void *) - 2] = reinterpret_cast<void *>(Nova::reply_and_wait_fast);
    Logging::printf("\t\tcreate ec id %d stack %p utcb %p at %p = %p tls %x\n", 
		    _cap_free, stack, utcb, stack + stack_size/sizeof(void *) -  STACK_FRAME, stack[stack_size/sizeof(void *) -  STACK_FRAME], tls);
    check(create_ec(_cap_free, utcb,  stack + stack_size/sizeof(void *) -  STACK_FRAME));
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
    check(hip_calc_checksum(hip));
    _hip = hip;
    _boot_utcb = reinterpret_cast<Utcb *>(hip) - 1;
    _cap_free = hip->pre + hip->gsi;
    Nova::create_sm(_cap_block = _cap_free++, 0);

    // prepopulate phys+virt memory
    extern char __image_start, __image_end;
    _free_virt.add(Region(VIRT_START, reinterpret_cast<unsigned long>(_boot_utcb) - VIRT_START));
    _free_virt.del(Region(reinterpret_cast<unsigned long>(&__image_start), &__image_end - &__image_start));
    _virt_phys.add(Region(reinterpret_cast<unsigned long>(&__image_start), &__image_end - &__image_start,
			  Nova::hip_module(hip, 0)->address));
    return 0;
  };

  /**
   * Block ourself.
   */
  void __attribute__((noreturn)) block_forever() { while (1) Nova::semdown(_cap_block); };

  static void debug_ec_name(const char *prefix, unsigned value)
  {
    // XXX Using reserved field in UTCB
    Utcb *u = myutcb();
    u->head._reserved = 0;
    static const char hex[] = "01234567890abcdef";
    for (unsigned i=0; i < 4; i++)
      {
	u->head._reserved |= (hex[value & 0xf] & 0xff) << 8*(3-i);
	value >>= 4;
      }
    for (unsigned i=0; i < 4 && prefix[i]; i++)
      u->head._reserved = (u->head._reserved & ~(0xff << i*8)) | (prefix[i] << i*8);
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
