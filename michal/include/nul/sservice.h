// -*- Mode: C++ -*-
/**
 * @file
 * Simple service base class - provides code common to most services.
 *
 * Copyright (C) 2011, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Copyright (C) 2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include <nul/parent.h>
#include <nul/generic_service.h>
#include <nul/baseprogram.h>

class BaseSService {

protected:
  virtual cap_sel alloc_cap(unsigned count = 1) = 0;
  virtual void  dealloc_cap(cap_sel c) = 0;
  virtual cap_sel create_ec4pt(phy_cpu_no cpu, Utcb **utcb_out) = 0;

  template<class S>
  void check_clients(Utcb &utcb, S &sessions) {
    typename S::CapAllocator* cap_allocator = static_cast<typename S::CapAllocator*>(this);
    typename S::Guard guard_c(&sessions, utcb, cap_allocator);
    typename S::ClientData volatile * data = sessions.get_invalid_client(utcb, cap_allocator);
    while (data) {
      Logging::printf("ad: found dead client - freeing datastructure\n");
      sessions.free_client_data(utcb, data, cap_allocator);
      data = sessions.get_invalid_client(utcb, cap_allocator, data);
    }
  }

public:
  unsigned alloc_crd()
  {
    return Crd(alloc_cap(), 0, DESC_CAP_ALL).value();
  }


  /**
   * Registers the service with parent
   *
   * @param service Service class
   * @param service_name Service name to register
   * @param hip Specifies on which CPUs to start the service.
   *
   * @return
   */
  template<class T> // We use the template to properly instantiate StaticPortalFunc
  static bool register_service(T *service, const char *service_name, Hip &hip)
  {
    Logging::printf("Constructing service %s...\n", service_name);

    cap_sel  service_cap = service->alloc_cap();
    unsigned res;
    mword portal_func = reinterpret_cast<mword>(StaticPortalFunc<T>::portal_func);

    for (phy_cpu_no i = 0; i < hip.cpu_desc_count(); i++) {
      Hip_cpu const &cpu = hip.cpus()[i];
      if (not cpu.enabled()) continue;

      // Create service EC
      Utcb *utcb_service;
      cap_sel worker_ec = service->create_ec4pt(i, &utcb_service);
      assert(worker_ec != 0);
      utcb_service->head.crd = service->alloc_crd();
      utcb_service->head.crd_translate = Crd(0, 31, DESC_CAP_ALL).value();

      cap_sel worker_pt = service->alloc_cap();
      res = nova_create_pt(worker_pt, worker_ec, portal_func, 0);
      assert(res == NOVA_ESUCCESS);

      // Register service
      res = ParentProtocol::register_service(*BaseProgram::myutcb(), service_name, i,
                                             worker_pt, service_cap);
      if (res != ENONE)
        Logging::panic("Registering service on CPU%u failed.\n", i);
    }

    Logging::printf("Service %s registered.\n", service_name);
    return true;
  }

  template<class S>
  unsigned handle_sessions(unsigned op, Utcb::Frame &input, S &sessions, bool &free_cap)
  {
    Utcb &utcb = *BaseProgram::myutcb();
    unsigned res;
    typename S::CapAllocator* cap_allocator = static_cast<typename S::CapAllocator*>(this);

    switch (op) {
    case ParentProtocol::TYPE_OPEN:
      {
        unsigned pseudonym = input.received_cap();
        unsigned cap_session = 0;

        if (!pseudonym) return EPROTO;

        //Ask parent whether we have already a session with this client (TODO: Not all services may want this)
        res = ParentProtocol::check_singleton(utcb, pseudonym, cap_session);
        if (!res && cap_session)
	  {
	    typename S::ClientData volatile *data = 0;
	    typename S::Guard guard_c(&sessions, utcb, cap_allocator);
	    while (data = sessions.next(data)) {
	      if (data->identity == cap_session) {
		dealloc_cap(data->pseudonym); //replace old pseudonym, first pseudnym we got via parent and gets obsolete as soon as client becomes running
		data->pseudonym = pseudonym;
		utcb << Utcb::TypedMapCap(data->identity);
		free_cap = false;
		check_clients(utcb, sessions);
		return ENONE;
	      }
	    }
	  }

	typename S::ClientData *data = 0;
        res = sessions.alloc_client_data(utcb, data, pseudonym,
					cap_allocator);
        if (res == ERESOURCE) { check_clients(utcb, sessions); return ERETRY; } //force garbage collection run
        else if (res) return res;

        res = ParentProtocol::set_singleton(utcb, data->pseudonym, data->identity);
        assert(!res);

        utcb << Utcb::TypedMapCap(data->identity);
        return ENONE;
      }
    case ParentProtocol::TYPE_CLOSE:
      {
        typename S::ClientData *data = 0;
        typename S::Guard guard_c(&sessions, utcb, cap_allocator);
        check1(res, res = sessions.get_client_data(utcb, data, input.identity()));
        return sessions.free_client_data(utcb, data, cap_allocator);
      }
    }
    return EPROTO;
  }
};
