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
#include <wvtest.h>

template<class Session, class A>
class BaseSService {
protected:
  typedef ClientDataStorage<Session, A> Sessions;
  Sessions _sessions;
  cap_sel  _worker_ec_base;
  char * flag_revoke; // Share memory with parent to signalize us dead clients

  virtual unsigned new_session(Session *session) = 0;
  virtual unsigned handle_request(Session *session, unsigned op, Utcb::Frame &input, Utcb &utcb, bool &free_cap) = 0;

  mword get_portal_func_addr() { return reinterpret_cast<mword>(StaticPortalFunc<A>::portal_func); }

public:

  // I don't like this CAP (de)alloc stuff here, but it is currently
  // the easist way how to implement it independently of the service.
  virtual cap_sel alloc_cap(unsigned count = 1) = 0;
  virtual void    dealloc_cap(cap_sel c) = 0;
  virtual cap_sel create_ec4pt(phy_cpu_no cpu, Utcb **utcb_out, cap_sel ec = ~0u) = 0;

  unsigned alloc_crd() {return Crd(alloc_cap(), 0, DESC_CAP_ALL).value();}

  /**
   * Registers the service with parent
   *
   * @param service_name Service name to register
   * @param hip Specifies on which CPUs to start the service.
   *
   * @return true in case of success, false otherwise.
   */
  bool register_service(const char *service_name, Hip &hip = Global::hip)
  {
    Logging::printf("Constructing service %s...\n", service_name);

    cap_sel  service_cap = alloc_cap();
    unsigned res;

    _worker_ec_base = alloc_cap(hip.cpu_desc_count());

    flag_revoke = new (0x1000) char[0x1000];
    if (!flag_revoke) return false;

    for (phy_cpu_no i = 0; i < hip.cpu_desc_count(); i++) {
      Hip_cpu const &cpu = hip.cpus()[i];
      if (not cpu.enabled()) continue;

      // Create service EC
      Utcb *utcb_service;
      cap_sel worker_ec = create_ec4pt(i, &utcb_service, _worker_ec_base+i);
      assert(worker_ec != 0);
      utcb_service->head.crd = alloc_crd();
      utcb_service->head.crd_translate = static_cast<A*>(this)->get_crdt();

      // TODO cap_sel worker_ec = _worker_ec_base+i;
      cap_sel worker_pt = alloc_cap();
      assert(worker_pt);
      res = nova_create_pt(worker_pt, _worker_ec_base+i, get_portal_func_addr(), 0);
      assert(res == NOVA_ESUCCESS);

      // Register service
      res = ParentProtocol::register_service(*BaseProgram::myutcb(), service_name, i,
                                             worker_pt, service_cap, flag_revoke);
      if (res != ENONE)
        Logging::panic("Registering service on CPU%u failed: %#x.\n", i, res);
    }

    Logging::printf("Service %s registered.\n", service_name);
    return true;
  }

  unsigned open_session(Utcb &utcb, cap_sel pseudonym, bool &free_cap)
  {
    unsigned res;
    unsigned cap_singleton = 0;

    if (!pseudonym) return EPROTO;

    //Ask parent whether we have already a session with this client (TODO: Not all services may want this)
    res = ParentProtocol::check_singleton(utcb, pseudonym, cap_singleton);
    if (!res && cap_singleton)
      {
        Session *session = 0;
        typename Sessions::Guard guard_c(&_sessions, utcb, static_cast<A*>(this));
        while (session = _sessions.next(session)) {
          if (session->get_singleton() == cap_singleton) {
            dealloc_cap(session->pseudonym); //replace old pseudonym, first pseudnym we got via parent and gets obsolete as soon as client becomes running
            session->pseudonym = pseudonym;

            if (!session->get_identity())
              res = _sessions.alloc_identity(session, static_cast<A*>(this));

            utcb << Utcb::TypedMapCap(session->get_identity());
            free_cap = false;
            _sessions.cleanup_clients(utcb, static_cast<A*>(this));
            return ENONE;
          }
        }
      }

    Session *session = 0;
    res = _sessions.alloc_client_data(utcb, session, pseudonym, static_cast<A*>(this));
    if (res == ERESOURCE) { _sessions.cleanup_clients(utcb, static_cast<A*>(this)); return ERETRY; } //force garbage collection run
    else if (res) return res;

    if (*flag_revoke) { *flag_revoke = 0; _sessions.cleanup_clients(utcb, static_cast<A*>(this)); }

    res = ParentProtocol::set_singleton(utcb, session->pseudonym, session->get_singleton());
    assert(!res);

    free_cap = false;
    utcb << Utcb::TypedMapCap(session->get_identity());
    return new_session(session);
  }

  unsigned close_session(Utcb &utcb, cap_sel session_id)
  {
    unsigned res;
    Session *session = 0;
    typename Sessions::Guard guard_c(&this->_sessions, utcb, static_cast<A*>(this));
    check1(res, res = _sessions.get_client_data(utcb, session, session_id));
    return _sessions.free_client_data(utcb, session, static_cast<A*>(this));
  }

  unsigned handle_session(Utcb &utcb, cap_sel session_id, unsigned op, Utcb::Frame &input, bool &free_cap)
  {
    unsigned res;
    typename Sessions::Guard guard_c(&this->_sessions);
    Session *session = 0;
    if (res = _sessions.get_client_data(utcb, session, session_id)) {
      // Return EEXISTS to ask the client for opening the session
      // Logging::printf("Cannot get client (id=0x%x) session: 0x%x\n", pid, res);
      return res;
    }
    return handle_request(session, op, input, utcb, free_cap);
  }

};

/// Service that uses semaphores as identifiers for sessions. This is
/// the original way how parent/service protocol worked. Kernel has to
/// perform translate operation during every call to the service.
template<class Session, class A>
class XlateSService : public BaseSService<Session, A> {
  typedef BaseSService<Session, A> Base;

public:
  unsigned get_crdt() { return Crd(0, 31, DESC_CAP_ALL).value(); }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
  {
    unsigned op;
    check1(EPROTO, input.get_word(op));

    switch (op) {
    case ParentProtocol::TYPE_OPEN:
      return Base::open_session(utcb, input.received_cap(), free_cap);
    case ParentProtocol::TYPE_CLOSE:
      return Base::close_session(utcb, input.identity());
    default:
      return Base::handle_session(utcb, input.identity(), op, input, free_cap);
    }
  }
};


/// Service that uses portals insted of semaphores as identifiers for sessions
template<class Session, class A>
class NoXlateSService : public BaseSService<Session, A> {
  typedef BaseSService<Session, A> Base;
public:
  unsigned get_crdt() { return 0; }

  unsigned create_session_portal(cap_sel pt) {
    return nova_create_pt(pt, Base::_worker_ec_base + BaseProgram::myutcb()->head.nul_cpunr, Base::get_portal_func_addr(), 0);
  }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
  {
    unsigned op;
    check1(EPROTO, input.get_word(op));

    switch (op) {
     case ParentProtocol::TYPE_OPEN:
       return Base::open_session(utcb, input.received_cap(), free_cap);
    case ParentProtocol::TYPE_CLOSE:
      return Base::close_session(utcb, pid);
    default:
      return Base::handle_session(utcb, pid, op, input, free_cap);
    }
  }
};
