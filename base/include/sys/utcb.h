/*
 * User Thread Control Block (UTCB)
 *
 * Copyright (C) 2008, Udo Steinberg <udo@hypervisor.org>
 * Copyright (C) 2008-2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011, Alexander Boettcher <ab764283@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL.
 *
 * NUL is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 * License version 2 for more details.
 */

#pragma once
#include "desc.h"
#include "service/cpu.h"
#include "service/helper.h"
#include "service/string.h"
#include "service/logging.h"

struct Utcb
{
  enum {
    STACK_START = 512,
  };

  typedef struct Descriptor
  {
    unsigned short sel, ar;
    unsigned limit, base, res;
    void set(unsigned short _sel, unsigned _base, unsigned _limit, unsigned short _ar) { sel = _sel; base = _base; limit = _limit; ar = _ar; };
  } Descriptor;

  struct head {
    union {
      struct {
        unsigned short untyped;
        unsigned short typed;
      };
      unsigned mtr;
    };
    unsigned crd_translate;
    unsigned crd;
    unsigned nul_cpunr;
  } head;
  union {
    struct {
      unsigned     mtd;
      unsigned     inst_len, eip, efl;
      unsigned     intr_state, actv_state, inj_info, inj_error;
      union {
	struct {
#define GREG(NAME)					\
	  union {					\
	    struct {					\
	      unsigned char           NAME##l, NAME##h;	\
	    };						\
	    unsigned short          NAME##x;		\
	    unsigned           e##NAME##x;		\
	  }
#define GREG16(NAME)				\
	  union {				\
	    unsigned short          NAME;	\
	    unsigned           e##NAME;		\
	  }
	  GREG(a);    GREG(c);    GREG(d);    GREG(b);
	  GREG16(sp); GREG16(bp); GREG16(si); GREG16(di);
	};
	unsigned gpr[8];
      };
      unsigned long long qual[2];
      unsigned     ctrl[2];
      long long tsc_off;
      unsigned     cr0, cr2, cr3, cr4;
      unsigned     dr7, sysenter_cs, sysenter_esp, sysenter_eip;
      Descriptor   es, cs, ss, ds, fs, gs;
      Descriptor   ld, tr, gd, id;
    };
    unsigned msg[(4096 - sizeof(struct head)) / sizeof(unsigned)];
  };

  /**
   * A smaller frame on the UTCB. Can only be used to read incoming messages.
   */
  class Frame {
    Utcb *_utcb;
    unsigned _end;
    unsigned _consumed;
  public:

    /**
     * Returns the skip-th delegated capability range descriptor.
     */
    unsigned received_item(unsigned skip=0) {
      for (unsigned i=0; i < _utcb->head.typed; i++)
        if (_utcb->msg[_end - i * 2 - 1] & 1 && !skip--)
          return _utcb->msg[_end - i * 2 - 2];
      return 0;
    }

    /** 
     * Returns the first capability in the first range delegated to
     * us.
     */
    unsigned received_cap() {
      return Crd(received_item()).cap();
    }

    Crd translated_cap(unsigned skip=0) {
      for (unsigned i=0; i < _utcb->head.typed; i++)
        if (~_utcb->msg[_end - i * 2 - 1] & 1 && !skip--)
          return Crd(_utcb->msg[_end - i * 2 - 2]);
      return Crd(0);
    }

    unsigned identity(unsigned skip=0) {
      return translated_cap(skip).cap();
    }

    void dump_typed_items() {
      for (unsigned i=0; i < _utcb->head.typed; i++)
        Logging::printf("%x | %x - cap %x\n", _utcb->msg[_end - i * 2 - 1],
                                              _utcb->msg[_end - i * 2 - 2],
                                              Crd(_utcb->msg[_end - i * 2 - 2]).cap());
    }


    unsigned untyped() { return _utcb->head.untyped; }
    unsigned typed()   { return _utcb->head.typed; }
    unsigned get_crd() { return _utcb->head.crd; }

    /**
     * Copy an arbitrarily sized data item from the Frame and moves an
     * internal pointer to the next item.
     * @return true on error, false on success.
     */
    template <typename T>
    bool get_word(T &value) {
      unsigned words = (sizeof(T) + sizeof(unsigned) - 1) / sizeof(unsigned);
      if (_consumed + words > _utcb->head.untyped) return true;
      value = *reinterpret_cast<T *>(_utcb->msg+_consumed);
      _consumed += words;
      return false;
    }
    unsigned *get_ptr() {return _utcb->msg+_consumed;}
    unsigned short unconsumed() { return untyped() - _consumed; }
    char *get_string(unsigned &len) {
      if (_consumed >= _utcb->head.untyped) return 0;
      len = *(_utcb->msg + _consumed);
      _consumed += 1;
      char *res =  reinterpret_cast<char *>(_utcb->msg + _consumed);
      _consumed += (len + sizeof(unsigned) - 1) / sizeof(unsigned);
      if (_consumed > _utcb->head.untyped) {
        len = 0;
        return 0;
      }
      return res;
    }

    char *get_zero_string(unsigned &len) {
      char *res = get_string(len);
      if (res) {
        res[len - 1] = 0;
        len = strnlen(res, len);
      }
      return res;
    }

    Frame(Utcb *utcb, unsigned end) : _utcb(utcb), _end(end), _consumed() {}
  };

  Frame get_nested_frame() {
    if (!msg[STACK_START])
      return Frame(this, sizeof(this)/sizeof(unsigned));
    unsigned old_ofs = msg[msg[STACK_START] + STACK_START] + STACK_START + 1;
    Utcb *x = reinterpret_cast<Utcb *>(msg+old_ofs);
    return Frame(x, x->head.untyped + 2*x->head.typed);
  }

  /** Used with << operator to set up "delegate" typed item in UTCB. */
  struct TypedMapCap {
    unsigned value;
    void fill_words(unsigned *ptr, unsigned hotspot=MAP_MAP) {   *ptr++ = value;  *ptr = hotspot;  }
    TypedMapCap(unsigned cap, unsigned attr = DESC_CAP_ALL) : value(cap << MINSHIFT | attr) {}
  };

  /** Used with << operator  to set up "translate" typed item in UTCB. */
  struct TypedIdentifyCap {
    unsigned value;
    void fill_words(unsigned *ptr) {   *ptr++ = value;  *ptr = 0;  }
    TypedIdentifyCap(unsigned cap, unsigned attr = DESC_CAP_ALL) : value(cap << MINSHIFT | attr) {}
  };

  /** Used with << operator to set up "translate" typed item in UTCB for memory. */
  struct TypedTranslateMem : TypedIdentifyCap {
    TypedTranslateMem(void *base, unsigned order, unsigned perms = DESC_RIGHTS_ALL)
      : TypedIdentifyCap(0, reinterpret_cast<unsigned>(base) & ~0xfff | (order & 0x1f) << 7 | perms & DESC_RIGHTS_ALL | DESC_TYPE_MEM) {}
  };

  struct String {
    const char * value;
    unsigned long len;
    String (const char *_value, unsigned long _len = ~0UL) : value(_value), len(_len == ~0UL ? strlen(_value) + 1 : _len) {}
  };

  /**
   * Push UTCB header and data to a stack area in the UTCB.
   *
   * Later, UTCB can be fully restored by drop_frame() or partially
   * restored by skip_frame().
   *
   * TODO: put error code at some fixed point
   */
  Utcb &  add_frame() {
    unsigned ofs = msg[STACK_START] + STACK_START + 1;
    // XXX Security problem!
    assert(ofs < sizeof(msg)/sizeof(msg[0]));

    //Logging::printf("add  %p frame at %x %x/%x\n", this, ofs, head.untyped, head.typed);
    // save header, untyped and typed items
    memcpy(msg+ofs, &head, HEADER_SIZE);
    ofs += HEADER_SIZE/sizeof(unsigned);
    memcpy(msg + ofs, msg, head.untyped*sizeof(unsigned));
    ofs += head.untyped;
    memcpy(msg + ofs, msg + sizeof(msg) / sizeof(unsigned) - head.typed* 2, head.typed * 2 * 4);
    ofs += head.typed * 2;
    msg[ofs++] = msg[STACK_START];
    msg[STACK_START] = ofs - STACK_START - 1;

    assert(ofs < sizeof(msg) / sizeof(unsigned));

    // init the header
    head.mtr = 0;
    head.crd = ~0;
    head.crd_translate = 0;
    return *this;
  }

  // Restore UTCB header except MTR from saved frame.
  void skip_frame() {
    //Logging::printf("skip %p frame at %x-%x\n", this, ofs, msg[ofs - 1] + STACK_START + 1);
    unsigned mtr = head.mtr;
    unsigned ofs = msg[STACK_START] + STACK_START + 1;
    msg[STACK_START] = msg[ofs - 1];
    memcpy(&head, msg + msg[STACK_START] + STACK_START + 1 , HEADER_SIZE);
    head.mtr = mtr;
  }

  /**
   * Restore UTCB to the state saved by the last add_frame() and
   * remove the restored state from the stack area.
   */
  void drop_frame() {
    unsigned ofs = msg[STACK_START] + STACK_START + 1;
    assert (ofs > STACK_START + 1);

    // XXX clear the UTCB to detect bugs
    memset(msg, 0xe8, 512 * 4);
    memset(msg+ofs, 0xd5, sizeof(msg) - ofs * 4);

    unsigned old_ofs = msg[ofs - 1] +  STACK_START + 1;
    //Logging::printf("drop %p frame %x-%x\n", this, old_ofs, ofs);
    ofs = old_ofs;
    memcpy(&head, msg+ofs, HEADER_SIZE);
    ofs += HEADER_SIZE/sizeof(unsigned);
    memcpy(msg, msg+ofs, head.untyped * sizeof(unsigned));
    ofs += head.untyped;
    memcpy(msg + sizeof(msg) / sizeof(unsigned) - head.typed* 2, msg + ofs, head.typed * 2 * 4);
    ofs += head.typed * 2;
    msg[STACK_START] = old_ofs - STACK_START - 1;
  }

  void set_header(unsigned untyped, unsigned typed) { head.untyped = untyped; head.typed = typed; }
  unsigned *item_start() { return reinterpret_cast<unsigned *>(this + 1) - 2*head.typed; }

  Utcb &  operator <<(unsigned value) {
    msg[head.untyped++] = value;
    return *this;
  }

  // Optional check to avoid IPCs where receiver will reject
  // the message because of validate_recv_bounds() result.
  bool validate_send_bounds() {
    return (head.untyped*sizeof(unsigned) + HEADER_SIZE < STACK_START); //XXX there are more checks
  }

  // Check whether the UTCB is empty (e.g. after receiving a message
  // through a portal) and does not violate our message size
  // constraints.
  bool validate_recv_bounds()
  {
    return
      (msg[STACK_START] == 0) and
      (head.untyped*sizeof(unsigned) + HEADER_SIZE < STACK_START) and
      (head.typed*sizeof(unsigned)*2 < sizeof(struct Utcb)) and
      ((sizeof(struct Utcb) - head.typed*sizeof(unsigned)*2) > (HEADER_SIZE + STACK_START + (head.untyped + 1 + head.typed*2)*sizeof(unsigned)));
  }

  template <typename T>
  Utcb &  operator <<(T &value) {
    unsigned words = (sizeof(T) + sizeof(unsigned) - 1)/ sizeof(unsigned);
    *reinterpret_cast<T *>(msg + head.untyped) = value;
    head.untyped += words;
    return *this;
  }

  Utcb &  operator <<(String string) {
    // Ever send a extra byte so that receiver has place to put a 0 behind the string (see get_zero_string)
    // add len of string so that receiver has not to scan for the end of the string or has to rely on a 0 termination
    // len also required to stream words after a string
    unsigned olduntyped = head.untyped;
    unsigned slen = string.len + 1;
    msg[olduntyped] = slen;
    head.untyped += 1 + (slen + sizeof(unsigned) - 1 ) / sizeof(unsigned);
    msg[head.untyped - 1] = 0;
    memcpy(msg + olduntyped + 1, string.value, slen - 1);
    return *this;
  }

  Utcb &  operator <<(TypedMapCap value) {
    head.typed++;
    value.fill_words(item_start());
    return *this;
  }

  Utcb &  operator <<(TypedIdentifyCap value) {
    head.typed++;
    value.fill_words(item_start());
    return *this;
  }

  Utcb &  operator <<(Crd value) { head.crd = value.value(); return *this; }

  template <typename T>
  bool  operator >>(T &value) {
    unsigned offset = msg[0];
    unsigned words = (sizeof(T) + sizeof(unsigned) - 1)/ sizeof(unsigned);
    if (offset + words > head.untyped) return true;
    msg[0] += words;
    value = *reinterpret_cast<T *>(msg + offset + 1);
    return false;
  }

  // XXX
  enum { MINSHIFT = 12 };
  enum {
    HEADER_SIZE = sizeof(struct head),
  };

  /**
   * Add mappings to a UTCB.
   * Returns size of memory left which couldn't be put on the utcb becaus no space is left.
   */
  WARN_UNUSED
  unsigned long add_mappings(unsigned long addr, unsigned long size, unsigned long hotspot, unsigned rights,
                             bool frame = false, unsigned max_items = sizeof(msg) / sizeof(msg[0]))
  {
    while (size > 0) {
      unsigned minshift = Cpu::minshift(addr | (hotspot & ~0xffful) , size);
      assert(minshift >= Utcb::MINSHIFT);
      this->head.typed++;
      unsigned *item = this->item_start();
      if (item <= this->msg+this->head.untyped ||
          (frame ? item <= (this->msg + this->msg[STACK_START]) : false) ||
          (frame ? this->head.typed > max_items : false))
        { this->head.typed --; return size; }
      item[1] = hotspot;
      item[0] = addr | ((minshift-Utcb::MINSHIFT) << 7) | rights;
      unsigned long mapsize = 1 << minshift;
      size    -= mapsize;
      addr    += mapsize;
      hotspot += mapsize;
    }
    return size;
  }

  /**
   * If you mixing code which manipulates the utcb by its own and you use this Utcb/Frame code,
   * you have to fix up your utcb after the code manipulated the utcb by its own. Otherwise some of the
   * assertion in the Frame code will trigger because the Utcb/Frame code assumes it's the only
   * one who manipulates the utcb. In general avoid this mixing, however in sigma0 it's not done everywhere.
   */
  void reset() {
    head.mtr = 0;
    this->msg[STACK_START] = 0;
  }
};
enum {
  EXCEPTION_WORDS = 72,
};


