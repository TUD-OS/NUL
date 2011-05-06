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
  bool boot_finished;
  //end

  template<class T>
  class Tmp_a : public InternalCapAllocator {
    public:
    T * obj;
    unsigned cap;
    Tmp_a(T * _obj) : obj(_obj), cap(0) {}
    unsigned alloc_cap(unsigned n = 1) {
      assert(n == 1);
      assert(cap == 0);
      cap = obj->alloc_cap();
      return cap;
    }
  };

public:
  template<class T>
  unsigned alloc_sc(Utcb &utcb, unsigned idx_ec, struct para p, unsigned cpu, T * _obj, char const * name, bool a_sc = false) {
    unsigned res;

    if (boot_finished)
      res = AdmissionProtocol::alloc_sc(utcb, idx_ec, p, cpu, _obj, name);
    else {
      Tmp_a<T> obj(_obj);
      res = AdmissionProtocol::alloc_sc(utcb, idx_ec, p, cpu, &obj, name);
      assert(!res && counter < tmp_size);
      tmp[counter].para = p; tmp[counter].cpu = cpu; tmp[counter].idx = obj.cap; tmp[counter].admission_sc = a_sc;
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

    if (root_sc != ~0U) {
      assert(tmp && counter < tmp_size);
      assert(root_cpu != ~0U);
      //XXX guessing quantum of root SC required ;-(
      tmp[counter].para = sched(); tmp[counter].cpu = root_cpu; tmp[counter].idx = root_sc; tmp[counter].admission_sc = false;
      memcpy(tmp[counter++].name, "main", 5);
    }

    for (i=0; i<counter; i++) {    
      res = call_server(init_frame(utcb, TYPE_SC_PUSH)
            << Utcb::TypedMapCap(tmp[i].idx) << tmp[i].para << tmp[i].cpu << Utcb::String(tmp[i].name) << tmp[i].admission_sc, true);
      if (res != ENONE) return res;
    }
    delete [] tmp;
    tmp = 0; counter = 0;
    _blocking = true; //enable blocking - early boot finished
    return ENONE;
  }

  explicit s0_AdmissionProtocol(unsigned cap_base, bool buffer, unsigned num=32) : AdmissionProtocol(cap_base, 0, false), boot_finished(!buffer)
  {
    if (!buffer) return;
    tmp_size = num;
    tmp = new struct tmp[num];
  }
};
