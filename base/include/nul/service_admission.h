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

#include "nul/capalloc.h"
#include "parent.h"

struct AdmissionProtocol : public GenericProtocol {

  typedef struct para {
    enum type { TYPE_APERIODIC = 1, TYPE_PERIODIC, TYPE_SPORADIC, TYPE_SYSTEM} type; //XXX TYPE_SYSTEM unnecessary, here to support old legacy prio layout
    para(enum type _type = TYPE_APERIODIC) : type(_type), wcet(0), period(0) {}
    unsigned wcet;
    unsigned period;
    //unsigned rel_deadline; //rel_deadline
    //unsigned rtime; //release time
    //unsigned mit; (minimal interarrival time)
  } sched;

  enum {
    TYPE_SC_ALLOC = ParentProtocol::TYPE_GENERIC_END,
    TYPE_SC_USAGE,
    TYPE_SET_NAME,
    TYPE_SC_PUSH,
  };

  template<class T>
  unsigned alloc_sc(Utcb &utcb, unsigned idx_ec, struct para p,
                    unsigned cpu, T * obj, char const * name) //obj is legacy
  {
    unsigned res = call_server(init_frame(utcb, TYPE_SC_ALLOC)
      << Utcb::TypedMapCap(idx_ec, DESC_TYPE_CAP | DESC_RIGHT_SC) << p << cpu << Utcb::String(name), true);
    if (res != ENONE && obj) {
      unsigned idx_sc = obj->alloc_cap();
      if (!idx_sc) return ERESOURCE;

      //LEGACY support, guessing that it could be vancouver and adjust prios
      if (p.type != sched::TYPE_APERIODIC) p.type = sched::TYPE_PERIODIC; //fix max prio to 2

      Qpd q(p.type, 10000); //EARLY SIGMA0 BOOT AND LEGACY SUPPORT (no admission service running), legacy support will vanish
      res = nova_create_sc (idx_sc, idx_ec, q);
      //Logging::printf("   cpu=%u cap=0x%x prio=%u quantum=%u\n", cpu, idx_sc, p.type, 10000);
    }
    return res;
  }


  unsigned alloc_sc(Utcb &utcb, unsigned idx_ec, struct para q, unsigned cpu, char const * name) {
    CapAllocator<AdmissionProtocol> * obj = 0;
    return alloc_sc(utcb, idx_ec, q, cpu, obj, name);
  }

  unsigned get_statistics(Utcb &utcb) { //XXX experimental frame must be dropped by caller
    return call_server(init_frame(utcb, TYPE_SC_USAGE) << Utcb::TypedIdentifyCap(_cap_base + CAP_SERVER_SESSION), false);
  }

  unsigned get_pseudonym(Utcb &utcb, unsigned client_id) {
    return ParentProtocol::get_pseudonym(utcb, _service, _instance, _cap_base + CAP_PSEUDONYM, client_id);
  }

  unsigned set_name(Utcb &utcb, char const * name, unsigned long name_len = ~0UL) {
    return call_server(init_frame(utcb, TYPE_SET_NAME) << Utcb::String(name, name_len), true);
  }

  explicit AdmissionProtocol(unsigned cap_base, unsigned instance=0, bool blocking = true) : GenericProtocol("admission", instance, cap_base, blocking) {}
};
