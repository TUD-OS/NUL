/*
 * Copyright (C) 2011, Alexander Boettcher <boettcher@tudos.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
#pragma once

#include "nul/generic_service.h"
#include "nul/capalloc.h"

struct AdmissionProtocol : public GenericProtocol {

  typedef struct para {
    enum type { TYPE_NONPERIODIC = 1, TYPE_PERIODIC, TYPE_APERIODIC, TYPE_SYSTEM} type; //XXX TYPE_SYSTEM unnecessary, here to support old legacy prio layout
    para(enum type _type = TYPE_NONPERIODIC) : type(_type) {}
  } sched;
  struct tmp {
    struct para para;
    unsigned cpu;
    unsigned idx; 
  };
  struct tmp * tmp;
  unsigned tmp_size;
  unsigned counter;

  enum {
    TYPE_SC_ALLOC = ParentProtocol::TYPE_GENERIC_END,
    TYPE_SC_USAGE,
    TYPE_SC_PUSH,
    TYPE_SET_NAME,
  };

  template<class T>
  unsigned alloc_sc(Utcb &utcb, unsigned idx_ec, struct para p, unsigned cpu, T * obj) {
    unsigned res = call_server(init_frame(utcb, TYPE_SC_ALLOC) << Utcb::TypedMapCap(idx_ec, DESC_TYPE_CAP | DESC_RIGHT_SC) << p << cpu, true);
    if (res != ENONE && obj) {
      unsigned idx_sc = obj->alloc_cap();
      if (!idx_sc) return ERESOURCE;

      if (!tmp_size) { //LEGACY support, guessing that it could be vancouver and adjust prios
        if (p.type != sched::TYPE_NONPERIODIC) p.type = sched::TYPE_PERIODIC; //fix max prio to 2
      }
      Qpd q(p.type, 10000); //EARLY SIGMA0 BOOT AND LEGACY SUPPORT (no admission service running), legacy support will vanish
      res = nova_create_sc (idx_sc, idx_ec, q);
      if (res == ENONE && tmp) {
        assert(counter < tmp_size);
        tmp[counter++] = { p, cpu, idx_sc };
      }
      //Logging::printf("   cpu=%u cap=0x%x prio=%u quantum=%u tmp_size=%u\n", cpu, idx_sc, p.type, 10000, tmp_size);
    }
/*
    if (counter) {
      unsigned i;
      for (i=0; i < counter; i++)
        Logging::printf("%u cpu=%u cap=0x%x prio=%u quantum=%u\n", i, tmp[i].cpu, tmp[i].idx, tmp[i].para. type, 10000);
    }
*/
    return res;
  }

  unsigned alloc_sc(Utcb &utcb, unsigned idx_ec, struct para q, unsigned cpu) {
    CapAllocator<AdmissionProtocol> * obj = 0;
    return alloc_sc(utcb, idx_ec, q, cpu, obj);
  }

  unsigned get_statistics(Utcb &utcb) { //XXX experimental frame must be dropped by caller
    return call_server(init_frame(utcb, TYPE_SC_USAGE) << Utcb::TypedIdentifyCap(_cap_base + CAP_SERVER_SESSION), false);
  }

  unsigned push_scs(Utcb &utcb) {
    unsigned res, i;

    for (i=0; i<counter; i++) {    
      res = call_server(init_frame(utcb, TYPE_SC_PUSH)
            << Utcb::TypedMapCap(tmp[i].idx) << tmp[i].para << tmp[i].cpu, true);
      if (res != ENONE) return res;
    }
    delete [] tmp;
    tmp = 0; counter = 0;
    return ENONE;
  }

  unsigned set_name(Utcb &utcb, char const * name, unsigned long name_len = ~0UL) {
    return call_server(init_frame(utcb, TYPE_SET_NAME) << Utcb::String(name, name_len), true);
  }

  unsigned get_pseudonym(Utcb &utcb, unsigned client_id) {
    return ParentProtocol::get_pseudonym(utcb, _service, _instance, _cap_base + CAP_PSEUDONYM, client_id);
  }

  explicit AdmissionProtocol(unsigned cap_base, unsigned instance=0) : GenericProtocol("admission", instance, cap_base, true) {}
  explicit AdmissionProtocol(unsigned cap_base, bool buffer, unsigned num=32) : GenericProtocol("admission", 0, cap_base, false)
  {
    if (!buffer) return;
    tmp_size = num;
    tmp = new struct tmp[num];
  }
};
