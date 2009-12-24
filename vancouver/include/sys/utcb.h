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

#include "sys/hip.h"
#include "sys/desc.h"
#include "vmm/cpu.h"
#include "driver/logging.h"
struct Utcb
{
  typedef struct Descriptor
  {
    unsigned short sel, ar;
    unsigned limit, base, res;
    void set(unsigned short _sel, unsigned _base, unsigned _limit, unsigned short _ar) { sel = _sel; base = _base; limit = _limit; ar = _ar; };
  } Descriptor;


  struct head {
    unsigned pid;
    Mtd mtr;
    unsigned crd;
    unsigned res0;
    unsigned res1;
    unsigned tls;
  } head;
  enum {
    HEADER_SIZE = sizeof(struct head),
  };

#define GREG(NAME)				\
  union {					\
    struct {					\
      uint8           NAME##l, NAME##h;		\
    };						\
    uint16          NAME##x;			\
    uint32           e##NAME##x;		\
  };
#define GREG16(NAME)				\
  union {					\
    uint16          NAME;			\
    uint32           e##NAME;			\
  };

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
	unsigned gpr[];
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
      unsigned     items[];
    };
    unsigned msg[1024 - sizeof(struct head) / sizeof(unsigned)];
  };

  enum { MINSHIFT = 12, };
  unsigned long add_mappings(bool exception, unsigned long addr, unsigned long size, unsigned long hotspot, unsigned rights)
  {
    while (size > 0)
      {
	unsigned minshift = Cpu::minshift(addr | hotspot, size);
	//assert(minshift >= MINSHIFT);
	unsigned *item = (exception ? items : (msg + head.mtr.untyped())) + head.mtr.typed() * 2;
	if (reinterpret_cast<Utcb *>(item) >= this+1 || head.mtr.typed() >= 255) return size;
	item[0] = hotspot;
	item[1] = addr | ((minshift-MINSHIFT) << 7) | rights;
	head.mtr = Mtd(head.mtr.untyped(), head.mtr.typed() + 1);

	unsigned long mapsize = 1 << minshift;
	size    -= mapsize;
	addr    += mapsize;
	hotspot += mapsize;
      }
    return size;
  };
};
