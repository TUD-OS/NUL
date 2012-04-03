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
    TYPE_GET_USAGE_CAP,
    TYPE_REBIND_USAGE_CAP,
    TYPE_SET_NAME,
    TYPE_SC_PUSH,
  };

  unsigned alloc_sc(Utcb &utcb, unsigned idx_ec, struct para p,
                    unsigned cpu, char const * name)
  {
    return call_server(init_frame(utcb, TYPE_SC_ALLOC)
      << Utcb::TypedMapCap(idx_ec, DESC_TYPE_CAP | DESC_RIGHT_SC) << p << cpu << Utcb::String(name), true);
  }

  /*
   * Returns subsumed time (since creation) of all SCs on all CPUs or of one named SC of a client
   * cap_sel client - capability obtained by using get_usage_cap method
   * uint64 con_tim - time value spent in microseconds since SC was created
   */
  unsigned get_statistics(Utcb &utcb, cap_sel client, uint64 &con_time, const char * name = "") {
    if (!client) return EPERM;
    unsigned res = call_server(init_frame(utcb, TYPE_SC_USAGE) << Utcb::TypedIdentifyCap(client) << Utcb::String(name), false);
    utcb >> con_time;
    utcb.drop_frame();
    return res;
  }

  unsigned rebind_usage_cap(Utcb &utcb, cap_sel client) {
    cap_sel crdout;
    Crd client_crd = Crd(client, 0, DESC_CAP_ALL);
    if (!client) return EPERM;
    unsigned char res = nova_syscall(NOVA_LOOKUP, client_crd.value(), 0, 0, 0, &crdout);
    assert(res == NOVA_ESUCCESS && crdout != 0); //sanity check for used cap;
    return call_server(init_frame(utcb, TYPE_REBIND_USAGE_CAP) << Utcb::TypedIdentifyCap(client) << client_crd, true);
  }

  unsigned get_usage_cap(Utcb &utcb, cap_sel client) {
    cap_sel crdout;
    Crd client_crd = Crd(client, 0, DESC_CAP_ALL);
    if (!client) return EPERM;
    unsigned char res = nova_syscall(NOVA_LOOKUP, client_crd.value(), 0, 0, 0, &crdout);
    assert(res == NOVA_ESUCCESS && crdout == 0); //sanity check for unused cap;
    return call_server(init_frame(utcb, TYPE_GET_USAGE_CAP) << client_crd, true);
  }

  unsigned get_pseudonym(Utcb &utcb, unsigned client_id) {
    return ParentProtocol::get_pseudonym(utcb, _service, _instance, _cap_base + CAP_PSEUDONYM, client_id);
  }

  unsigned set_name(Utcb &utcb, char const * name, unsigned long name_len = ~0UL) {
    return call_server(init_frame(utcb, TYPE_SET_NAME) << Utcb::String(name, name_len), true);
  }

  explicit AdmissionProtocol(unsigned cap_base, unsigned instance=0, bool blocking = true) : GenericProtocol("admission", instance, cap_base, blocking) {}
};
