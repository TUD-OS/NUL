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

#include "nul/service_admission.h"

struct s0_AdmissionProtocol : public AdmissionProtocol {

private:

  //start - data structures are only used during boot of s0 ! - never use it concurrently
  struct tmp {
    struct para para;
    unsigned cpu;
    unsigned idx; 
    bool admission_sc;
    char name [32];
  };
  struct tmp * tmp;
  unsigned tmp_size;
  unsigned counter;
  //end

public:
  template<class T>
  unsigned alloc_sc(Utcb &utcb, unsigned idx_ec, struct para p, unsigned cpu, T * _obj, char const * name, bool a_sc = false) {
    unsigned res;

    if (_blocking)
      res = AdmissionProtocol::alloc_sc(utcb, idx_ec, p, cpu, name);
    else {
      unsigned idx_sc = _obj->alloc_cap();
      if (!idx_sc) return ERESOURCE;

      Qpd q(p.type, 10000); //EARLY SIGMA0 BOOT (no admission service running)
      res = nova_create_sc (idx_sc, idx_ec, q, cpu);
      assert(!res && counter < tmp_size);
      tmp[counter].para = p; tmp[counter].cpu = cpu; tmp[counter].idx = idx_sc; tmp[counter].admission_sc = a_sc;
      memcpy(tmp[counter++].name, name, strlen(name) + 1);
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

  unsigned push_scs(Utcb &utcb, unsigned root_sc = ~0U, unsigned root_cpu = ~0U) {
    unsigned res, i;

    _blocking = true; //enable blocking - early boot finished

    nova_revoke(Crd(NOVA_DEFAULT_PD_CAP, 0, DESC_TYPE_CAP | DESC_RIGHT_SC), true); // revoke right to create sc by s0

    if (root_sc != ~0U) {
      assert(tmp && counter < tmp_size);
      assert(root_cpu != ~0U);
      //XXX guessing quantum of root SC required ;-(
      tmp[counter].para = sched(); tmp[counter].cpu = root_cpu; tmp[counter].idx = root_sc; tmp[counter].admission_sc = false;
      memcpy(tmp[counter++].name, "main", 5);
    }

    for (i=0; i<counter; i++) {    
      assert(tmp[i].idx != 0);
      res = call_server(init_frame(utcb, TYPE_SC_PUSH)
            << Utcb::TypedMapCap(tmp[i].idx) << tmp[i].para << tmp[i].cpu << Utcb::String(tmp[i].name) << tmp[i].admission_sc, true);
      if (res != ENONE) return res;
    }
    delete [] tmp;
    tmp = 0; counter = 0;

    return ENONE;
  }

  explicit s0_AdmissionProtocol(unsigned cap_base, bool buffer, unsigned num=32) : AdmissionProtocol(cap_base, 0, !buffer)
  {
    if (!buffer) return;
    tmp_size = num;
    tmp = new struct tmp[num];
  }
};
