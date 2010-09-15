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
#include "service/string.h"
#include "service/logging.h"

enum Eflags {
  EFL_CARRY = 1U << 0,
  EFL_ZERO  = 1U << 6,
};

struct Utcb
{
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
    Mtd      mtr_out;
    unsigned res1;
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


  unsigned get_received_cap() {
    for (unsigned i=0; i < head.mtr.typed(); i++)
      if (msg[sizeof(msg) / sizeof(unsigned) - i*2 - 1] & 1)
	return head.crd >> Utcb::MINSHIFT;
    return 0;
  }
  unsigned get_identity(unsigned skip=0) {
    for (unsigned i=0; i < head.mtr.typed(); i++)
      if (~msg[sizeof(msg) / sizeof(unsigned) - i*2 - 1] & 1 && !skip--)
	return msg[sizeof(msg) / sizeof(unsigned) - i*2- 2] >> Utcb::MINSHIFT;
    return 0;
  }

  enum { MINSHIFT = 12 };


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

  Utcb &  init_frame() { head.mtr_out = Mtd(0,0); return *this; }

  Utcb &  operator <<(unsigned value) {
    msg[head.mtr_out.untyped()] = value;
    head.mtr_out.add_untyped();
    return *this;
  }

  Utcb &  operator <<(const char *value) {
    unsigned olduntyped = head.mtr_out.untyped();
    unsigned slen =  strlen(value) + 1;
    head.mtr_out.add_untyped((slen + sizeof(unsigned) - 1 ) / sizeof(unsigned));
    msg[head.mtr_out.untyped() - 1] = 0;
    memcpy(msg + olduntyped, value, slen);
    return *this;
  }

  Utcb &  operator <<(TypedMapCap value) {
    head.mtr_out.add_typed();
    value.fill_words(msg + sizeof(msg) / sizeof(unsigned) - head.mtr_out.typed()* 2);
    return *this;
  }

  Utcb &  operator <<(TypedIdentifyCap value) {
    head.mtr_out.add_typed();
    value.fill_words(msg+sizeof(msg) / sizeof(unsigned) - head.mtr_out.typed()* 2);
    return *this;
  }

  Utcb &  operator <<(Crd value) { head.crd = value.value(); return *this; }
};
