/* -*- Mode: C++ -*- */

#pragma once

#include <nova/types.h>

NOVA_BEGIN

typedef struct Descriptor {
  uint16_t sel;
  uint16_t ar;
  uint32_t limit;
  uint32_t base;
  uint32_t _reserved;

#ifdef __cplusplus
  void set(unsigned short _sel, unsigned _base, unsigned _limit, unsigned short _ar) { sel = _sel; base = _base; limit = _limit; ar = _ar; };
#endif

} NOVA_PACKED Descriptor;

#define NOVA_GREG(NAME)				\
  union {					\
    struct {					\
      uint8_t NAME##l, NAME##h;			\
    };						\
    uint16_t  NAME##x;				\
    uint32_t  e##NAME##x;			\
  };
#define NOVA_GREG16(NAME)			\
  union {					\
    uint16_t  NAME;				\
    uint32_t           e##NAME;			\
  };


typedef struct Utcb_head {
  Cap_idx   pid;
  Mtd       mtr;
  Crd       crd;
  Cap_idx   exc;
  uint32_t  _reserved;
  uint32_t  tls;
} NOVA_PACKED Utcb_head;

typedef struct Utcb
{
  Utcb_head head;
  union {
    struct {
      union {
	struct {
	  NOVA_GREG(a);
	  NOVA_GREG(c);
	  NOVA_GREG(d);
	  NOVA_GREG(b);

	  NOVA_GREG16(sp);
	  NOVA_GREG16(bp);
	  NOVA_GREG16(si);
	  NOVA_GREG16(di);
	};
	uint32_t gpr[8];
      };

      uint32_t efl;
      uint32_t eip;
      uint32_t cr0;
      uint32_t cr2;

      uint32_t cr3;
      uint32_t cr4;
      uint32_t _reserved1;
      uint32_t dr7;

      Descriptor es;
      Descriptor cs;
      Descriptor ss;
      Descriptor ds;
      Descriptor fs;
      Descriptor gs;
      Descriptor ld;
      Descriptor tr;
      Descriptor gd;
      Descriptor id;

      uint32_t inj_info;
      uint32_t inj_error;
      uint32_t intr_state;
      uint32_t actv_state;
      uint64_t qual[2];
      uint32_t ctrl[2];
      uint64_t tsc_offset;
      uint32_t inst_len;

      uint32_t sysenter_cs;
      uint32_t sysenter_esp;
      uint32_t sysenter_eip;

      uint32_t items[];
    };
    uint32_t msg[1024 - sizeof(struct Utcb_head) / sizeof(uint32_t)];
  };
} NOVA_PACKED Utcb;

enum { MAPMINSHIFT = 12, };
NOVA_EXTERN_C uint32_t utcb_add_mappings(Utcb *utcb, bool exception, uint32_t addr,
					 uint32_t size, uint32_t hotspot, unsigned rights);

NOVA_END

/* EOF */
