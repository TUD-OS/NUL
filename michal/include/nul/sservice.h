// -*- Mode: C++ -*-
/**
 * @file
 * Simple service base class - provides code common to most services.
 *
 * Copyright (C) 2011, 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
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

template <class T, class A>
struct DefaultSessions : public ClientDataStorage<T, A> {};

template<class Session, template <class, class> class SessionsTmpl=DefaultSessions>
class BaseSService {
public:
  typedef SessionsTmpl<Session, BaseSService> Sessions;
  Sessions _sessions;
protected:
  virtual cap_sel create_ec4pt(phy_cpu_no cpu, Utcb **utcb_out) = 0;

  virtual unsigned new_session(Session *session) = 0;
  virtual unsigned handle_request(Session *session, unsigned op, Utcb::Frame &input, Utcb &utcb, bool &free_cap) = 0;

public:
  // We act as a cap allocator
  virtual cap_sel alloc_cap(unsigned count = 1) = 0;
  virtual void  dealloc_cap(cap_sel c) = 0;

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
  static bool register_service(T *service, const char *service_name, Hip &hip = Global::hip)
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

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
  {
    unsigned op, res;
    check1(EPROTO, input.get_word(op));

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
	    Session volatile *session = 0;
	    typename Sessions::Guard guard_c(&_sessions, utcb, this);
	    while (session = _sessions.next(session)) {
	      if (session->identity == cap_session) {
		dealloc_cap(session->pseudonym); //replace old pseudonym, first pseudnym we got via parent and gets obsolete as soon as client becomes running
		session->pseudonym = pseudonym;
		utcb << Utcb::TypedMapCap(session->identity);
		free_cap = false;
		_sessions.cleanup_clients(utcb, this);
		return ENONE;
	      }
	    }
	  }

	Session *session = 0;
        res = _sessions.alloc_client_data(utcb, session, pseudonym, this);
        if (res == ERESOURCE) { _sessions.cleanup_clients(utcb, this); return ERETRY; } //force garbage collection run
        else if (res) return res;

        res = ParentProtocol::set_singleton(utcb, session->pseudonym, session->identity);
        assert(!res);

        free_cap = false;
        utcb << Utcb::TypedMapCap(session->identity);
        return new_session(session);
      }
    case ParentProtocol::TYPE_CLOSE: {
      Session *session = 0;
      typename Sessions::Guard guard_c(&_sessions, utcb, this);
      check1(res, res = _sessions.get_client_data(utcb, session, input.identity()));
      return _sessions.free_client_data(utcb, session, this);
    }
    default:
      {
	typename Sessions::Guard guard_c(&_sessions, utcb, this);
	Session *session = 0;
	if (res = _sessions.get_client_data(utcb, session, input.identity())) {
	  // Return EEXISTS to ask the client for opening the session
	  Logging::printf("Cannot get client (id=0x%x) session: 0x%x\n", input.identity(), res);
	  return res;
	}
	return handle_request(session, op, input, utcb, free_cap);
      }
    }
  }
};

/// Service that uses portals insted of semaphores as identifiers for sessions
template<class Session, template <class, class> class SessionsTmpl=DefaultSessions>
class NoXlateSService : public BaseSService<Session, SessionsTmpl> {
  mword portal_func_addr;
  cap_sel worker_ec[Config::MAX_CPUS];
public:
  typedef BaseSService<Session, SessionsTmpl> Base;
  template<class T> // We use the template to properly instantiate StaticPortalFunc
  bool register_service(T *service, const char *service_name, Hip &hip)
  {
    Logging::printf("Constructing service %s...\n", service_name);

    cap_sel  service_cap = service->alloc_cap();
    unsigned res;
    service->portal_func_addr = reinterpret_cast<mword>(StaticPortalFunc<T>::portal_func);

    for (phy_cpu_no i = 0; i < hip.cpu_desc_count(); i++) {
      Hip_cpu const &cpu = hip.cpus()[i];
      if (not cpu.enabled()) continue;

      // Create service EC
      Utcb *utcb_service;
      cap_sel worker_ec = service->worker_ec[i] = service->create_ec4pt(i, &utcb_service);
      assert(worker_ec != 0);
      utcb_service->head.crd = service->alloc_crd();
      utcb_service->head.crd_translate = Crd(0, 31, DESC_CAP_ALL).value();

      cap_sel worker_pt = service->alloc_cap();
      res = nova_create_pt(worker_pt, worker_ec, service->portal_func_addr, 0);
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


  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
  {
    unsigned op, res;
    check1(EPROTO, input.get_word(op));

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
	    Session volatile *session = 0;
	    typename Base::Sessions::Guard guard_c(&this->_sessions, utcb, this);
	    while (session = Base::_sessions.next(session)) {
	      if (session->identity == cap_session) {
		dealloc_cap(session->pseudonym); //replace old pseudonym, first pseudnym we got via parent and gets obsolete as soon as client becomes running
		session->pseudonym = pseudonym;
		utcb << Utcb::TypedMapCap(session->identity);
		free_cap = false;
		this->cleanup_clients(utcb);
		return ENONE;
	      }
	    }
	  }

	Session *session = 0;
        res = Base::_sessions.alloc_client_data(utcb, session, pseudonym, this);
        if (res == ERESOURCE) { Base::cleanup_clients(utcb); return ERETRY; } //force garbage collection run
        else if (res) return res;

	// Throw away the semaphore and use a portal instead
	res = nova_revoke_self(Crd(session->identity, 0, DESC_CAP_ALL));
	assert(!res);
	cap_sel session_pt = this->alloc_cap();
	res = nova_create_pt(session_pt, worker_ec[utcb.head.nul_cpunr], portal_func_addr, 0);
	// TODO: We need per-CPU identity! For now let's hope that we are called only from one CPU.
	session->identity = session_pt;

        res = ParentProtocol::set_singleton(utcb, session->pseudonym, session->identity);
        assert(!res);

        free_cap = false;
        utcb << Utcb::TypedMapCap(session->identity);
        return new_session(session);
      }
    case ParentProtocol::TYPE_CLOSE: {
      Session *session = 0;
      typename Base::Sessions::Guard guard_c(&this->_sessions, utcb, this);
      check1(res, res = Base::_sessions.get_client_data(utcb, session, pid));
      return Base::_sessions.free_client_data(utcb, session, this);
    }
    default:
      {
	typename Base::Sessions::Guard guard_c(&this->_sessions, utcb, this);
	Session *session = 0;
	if (res = Base::_sessions.get_client_data(utcb, session, pid)) {
	  // Return EEXISTS to ask the client for opening the session
	  Logging::printf("Cannot get client (id=0x%x) session: 0x%x\n", pid, res);
	  return res;
	}
	return handle_request(session, op, input, utcb, free_cap);
      }
    }
  }
};
