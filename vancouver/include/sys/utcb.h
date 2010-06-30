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
    unsigned res0;
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
      unsigned     items[1];
    };
    unsigned msg[1024 - sizeof(struct head) / sizeof(unsigned)];
  };

  enum { MINSHIFT = 12 };
};
