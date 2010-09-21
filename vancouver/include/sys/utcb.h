/*
 * \file    $Id: utcb.h 1328 2007-12-18 23:59:18Z uas $
 * \brief   User Thread Control Block (UTCB)
 * \author  Udo Steinberg <udo@hypervisor.org>
 *
 * Copyright (C) 2005-2007, Udo Steinberg <udo@hypervisor.org>
 * Copyright (C) 2008-2009, Bernhard Kauer <bk@vmmon.org>
 * Technische Universitaet Dresden, Operating Systems Group
 * All rights reserved.
 */

#pragma once
#include "desc.h"
#include "service/cpu.h"
#include "service/helper.h"
#include "service/string.h"
#include "service/logging.h"

enum Eflags {
  EFL_CARRY = 1U << 0,
  EFL_ZERO  = 1U << 6,
};

struct Utcb
{
  enum {
    MINSHIFT = 12,
    STACK_START = 512,
  };

  typedef struct Descriptor
  {
    unsigned short sel, ar;
    unsigned limit, base, res;
    void set(unsigned short _sel, unsigned _base, unsigned _limit, unsigned short _ar) { sel = _sel; base = _base; limit = _limit; ar = _ar; };
  } Descriptor;


  struct head {
    unsigned res;
    Mtd      mtr;
    unsigned crd;
    unsigned res1[2];
    unsigned tls;
  } head;
  enum {
    HEADER_SIZE = sizeof(struct head)
  };

#define GREG(NAME)				\
  union {					\
    struct {					\
      unsigned char           NAME##l, NAME##h;		\
    };						\
    unsigned short          NAME##x;			\
    unsigned           e##NAME##x;		\
  }
#define GREG16(NAME)				\
  union {					\
    unsigned short          NAME;			\
    unsigned           e##NAME;			\
  }

  union {
    struct {
      union {
	struct {
	  GREG(a);
	  GREG(c);
	  GREG(d);
	  GREG(b);
	  GREG16(sp);
	  GREG16(bp);
	  GREG16(si);
	  GREG16(di);
	};
	unsigned gpr[8];
      };
      unsigned     efl;
      unsigned     eip;
      unsigned     cr0, cr2, cr3, cr4;
      unsigned     res;
      unsigned     dr7;
      Descriptor   es;
      Descriptor   cs;
      Descriptor   ss;
      Descriptor   ds;
      Descriptor   fs;
      Descriptor   gs;
      Descriptor   ld;
      Descriptor   tr;
      Descriptor   gd;
      Descriptor   id;
      unsigned     inj_info, inj_error;
      unsigned     intr_state, actv_state;
      unsigned long long qual[2];
      unsigned     ctrl[2];
      unsigned long long tsc_off;
      unsigned     inst_len;
      unsigned     sysenter_cs, sysenter_esp, sysenter_eip;
    };
    unsigned msg[1024 - sizeof(struct head) / sizeof(unsigned)];
  };

  unsigned *item_start() { return reinterpret_cast<unsigned *>(this + 1) - 2*head.mtr.typed(); }


  Mtd get_nested_mtr(unsigned &typed_offset) {
    if (!msg[STACK_START]) {
      typed_offset = sizeof(msg) / sizeof(unsigned) - 2 * head.mtr.typed();
      return head.mtr;
    }
    unsigned old_ofs = msg[msg[STACK_START] + STACK_START] + STACK_START + 1;
    typed_offset = old_ofs + 6 + Mtd(msg[old_ofs + 1]).size() / 4;
    return Mtd(msg[old_ofs + 1]);
  }


  unsigned get_received_cap() {
    unsigned typed_ofs;
    Mtd mtr = get_nested_mtr(typed_ofs);
    for (unsigned i=mtr.typed(); i > 0; i--)
      if (msg[typed_ofs + i * 2 - 1] & 1)
	return msg[typed_ofs + i * 2 - 2] >> Utcb::MINSHIFT;
    return 0;
  }

  unsigned get_identity(unsigned skip=0) {
    unsigned typed_ofs;
    Mtd mtr = get_nested_mtr(typed_ofs);
    //Logging::printf("iden %p %x %x %x\n", this, mtr.value(), typed_ofs, msg[STACK_START]);
    for (unsigned i=mtr.typed(); i > 0; i--)
      if (~msg[typed_ofs + i * 2 - 1] & 1 && !skip--)
	return msg[typed_ofs + i * 2 - 2] >> Utcb::MINSHIFT;
    return 0;
  }


  // MessageBuffer abstraction
  struct TypedMapCap {
    unsigned value;
    void fill_words(unsigned *ptr) {   *ptr++ = value;  *ptr = 1;  }
    TypedMapCap(unsigned cap, unsigned attr = DESC_CAP_ALL) : value(cap << MINSHIFT | attr) {}
  };

  struct TypedIdentifyCap {
    unsigned value;
    void fill_words(unsigned *ptr) {   *ptr++ = value;  *ptr = 0;  }
    TypedIdentifyCap(unsigned cap, unsigned attr = DESC_CAP_ALL) : value(cap << MINSHIFT | attr) {}
  };

  Utcb &  add_frame() {
    unsigned ofs = msg[STACK_START] + STACK_START + 1;

    //Logging::printf("add  %p frame at %x %x/%x\n", this, ofs, head.mtr.size() / 4, head.mtr.typed());
    // save header, untyped and typed items
    memcpy(msg+ofs, &head, 6 * 4);
    ofs += 6;
    memcpy(msg+ofs, msg, head.mtr.size());
    ofs += head.mtr.size() / 4;
    memcpy(msg+ofs, msg + sizeof(msg) / sizeof(unsigned) - head.mtr.typed()* 2, head.mtr.typed() * 2 * 4);
    ofs += head.mtr.typed() * 2;
    msg[ofs++] = msg[STACK_START];
    msg[STACK_START] = ofs - STACK_START - 1;

    assert(ofs < sizeof(msg) / sizeof(unsigned));

    // init them
    head.mtr = Mtd(0,0);
    head.crd = ~0;
    return *this;
  }

  unsigned skip_frame() {
    //Logging::printf("skip %p frame at %x-%x\n", this, ofs, msg[ofs - 1] + STACK_START + 1);
    unsigned res = head.mtr.value();
    unsigned ofs = msg[STACK_START] + STACK_START + 1;
    msg[STACK_START] = msg[ofs - 1];
    memcpy(&head, msg + msg[STACK_START] + STACK_START + 1 , 6 * 4);
    return res;
  }

  void drop_frame() {
    unsigned ofs = msg[STACK_START] + STACK_START + 1;
    assert (ofs > STACK_START + 1);

    // XXX clear the UTCB to detect bugs
    memset(msg, 0xe8, 512 * 4);
    memset(msg+ofs, 0xd5, sizeof(msg) - ofs * 4);

    unsigned old_ofs = msg[ofs - 1] +  STACK_START + 1;
    //Logging::printf("drop %p frame %x-%x\n", this, old_ofs, ofs);
    ofs = old_ofs;
    memcpy(&head, msg+ofs, 6 * 4);
    ofs += 6;
    memcpy(msg, msg+ofs, head.mtr.size());
    ofs += head.mtr.size();
    memcpy(msg + sizeof(msg) / sizeof(unsigned) - head.mtr.typed()* 2, msg+ofs, head.mtr.typed() * 2 * 4);
    ofs += head.mtr.typed() * 2;
    msg[STACK_START] = old_ofs - STACK_START - 1;
  }


  Utcb &  operator <<(unsigned value) {
    msg[head.mtr.untyped()] = value;
    head.mtr.add_untyped();
    return *this;
  }

  Utcb &  operator <<(const char *value) {
    unsigned olduntyped = head.mtr.untyped();
    unsigned slen =  strlen(value) + 1;
    head.mtr.add_untyped((slen + sizeof(unsigned) - 1 ) / sizeof(unsigned));
    msg[head.mtr.untyped() - 1] = 0;
    memcpy(msg + olduntyped, value, slen);
    return *this;
  }

  Utcb &  operator <<(TypedMapCap value) {
    head.mtr.add_typed();
    value.fill_words(msg + sizeof(msg) / sizeof(unsigned) - head.mtr.typed()* 2);
    return *this;
  }

  Utcb &  operator <<(TypedIdentifyCap value) {
    head.mtr.add_typed();
    value.fill_words(msg+sizeof(msg) / sizeof(unsigned) - head.mtr.typed()* 2);
    return *this;
  }

  Utcb &  operator <<(Crd value) { head.crd = value.value(); return *this; }
};
