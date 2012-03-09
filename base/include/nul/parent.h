/** @file
 * Parent protocol - constants and client side.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011, Alexander Boettcher <boettcher@tudos.org>
 * Copyright (C) 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
#pragma once

#include "sys/syscalls.h"
#include "sys/semaphore.h"
#include "nul/config.h"
#include "nul/error.h"

/**
 * The protocol our parent provides.  It allows to open a new session
 * and request per-CPU-portals.
 *
 * To be used by both clients and services.
 *
 * Missing:   restrict quota, get_quota
 */
struct ParentProtocol {

  /** Protocol operations (message types). */
  enum {
    // generic protocol operations (sent either to parent or to services)
    TYPE_INVALID = 0, ///< used as error indicator
    TYPE_OPEN, 	      ///< Get pseudonym (when sent to the parent) or open session (when sent to a service)
    TYPE_CLOSE,
    TYPE_GENERIC_END, ///< Marks the end of generic operations - service specific operations can start here.

    // Parent specific operations
    TYPE_GET_PORTAL,
    TYPE_REGISTER,
    TYPE_UNREGISTER,
    TYPE_GET_QUOTA,
    TYPE_SINGLETON,
    TYPE_REQ_KILL,
    TYPE_SIGNAL,
  };

  /** Capabilities used by parent to construct child */
  enum {
    /// Temporary SM capability used by parent during child
    /// construction. alloc_cap don't work here - translate window
    /// issue.
    CAP_CHILD_ID  = Config::CAP_PARENT_BEGIN,

    /// SM capability to get access to child utilization information
    /// provided by admission service.  Temporary capability - If this
    /// cap is rebound this index becomes invalid.
    CAP_SC_USAGE,

    /// EC capability of first child thread.
    CAP_CHILD_EC,

    /// Semaphore capability used by parent to identify children. Also
    /// used by child to signal events to parents
    /// (ParentProtocol::signal()).
    CAP_PARENT_ID,

    /// Portal capabilities (according to number of CPUs) passed by
    /// parent to child.
    CAP_PT_PERCPU,

    /// Idle SCs. Only created for the admission service.
    CAP_PT_IDLE_SCS = CAP_PT_PERCPU + Config::MAX_CPUS,
  };

  static_assert((CAP_PT_PERCPU + Config::MAX_CPUS) < (1U << Config::CAP_RESERVED_ORDER),
                "Capability Space misconfiguration.");

  static Utcb & init_frame(Utcb &utcb, unsigned op, unsigned id) {
    return utcb.add_frame() << op << Utcb::TypedIdentifyCap(Crd(id, 0, DESC_CAP_ALL)); }

  /**
   * Low-level systemcall.
   */
  static unsigned call(Utcb &utcb, unsigned cap_base, bool drop_frame, bool percpu = true) {
    unsigned res;
    res = nova_call(cap_base + (percpu ? utcb.head.nul_cpunr : 0));
    if (!res)
      if (!utcb.head.untyped) res = EPROTO; //if we don't get a result word it's a protocol violation
      else res = utcb.msg[0];
    if (drop_frame) utcb.drop_frame();
    return res;
  }

  static unsigned get_pseudonym(Utcb &utcb, const char *service, unsigned instance,
				unsigned cap_pseudonym, unsigned parent_id = CAP_PARENT_ID) {
    return call(init_frame(utcb, TYPE_OPEN, parent_id) << instance << Utcb::String(service)
						       << Crd(cap_pseudonym, 0, DESC_CAP_ALL),
		CAP_PT_PERCPU, true);
  }

  static unsigned release_pseudonym(Utcb &utcb, unsigned cap_pseudonym) {
    init_frame(utcb, TYPE_CLOSE, cap_pseudonym);
    return call(utcb, CAP_PT_PERCPU, true);
  }

  /** 
   * Ask parent to get the portal to talk to the service.
   */
  static unsigned get_portal(Utcb &utcb, unsigned cap_pseudonym, unsigned cap_portal, bool blocking, char const * service_name = 0) {

    init_frame(utcb, TYPE_GET_PORTAL, cap_pseudonym) << Crd(cap_portal, 0, DESC_CAP_ALL);
    unsigned res = call(utcb, CAP_PT_PERCPU, true);

    // block on the parent session until something happens
    if (res == ERETRY && blocking) {
      //Logging::printf("blocked - waiting for session %x %s\n", cap_pseudonym, service_name);
      res = nova_semdownmulti(cap_pseudonym);
      //Logging::printf("we woke up %u %s\n", res, service_name);
      if (res != ENONE) {
        res = nova_revoke(Crd(cap_pseudonym, 0, DESC_CAP_ALL), true);
        assert(res == ENONE);
      } else
        return ERETRY;
    }
    return res;
  }

  /**
   * @param cap_service Capability selector where parent delegates us
   * the service identifier. It seems that this identifier is not used
   * for anything.
   * @param revoke_mem Memory page the parent will use to signalize us
   * that some client has died (or closed session). The service can
   * use this to figure out when it is not necessary to search for
   * dead clients.
   */
  static unsigned
  register_service(Utcb &utcb, const char *service, unsigned cpu, unsigned pt,
                   unsigned cap_service, char * revoke_mem = 0)
  {
    assert(cap_service);
    init_frame(utcb, TYPE_REGISTER, CAP_PARENT_ID) << cpu << Utcb::String(service) << reinterpret_cast<unsigned>(revoke_mem)
						   << Utcb::TypedMapCap(Crd(pt, 0, DESC_CAP_ALL)) << Crd(cap_service, 0, DESC_CAP_ALL);
    return call(utcb, CAP_PT_PERCPU, true);
  };

  static unsigned unregister_service(Utcb &utcb, unsigned cap_service) {
    init_frame(utcb, TYPE_UNREGISTER, cap_service);
    return call(utcb, CAP_PT_PERCPU, true);
  }

  static unsigned get_quota(Utcb &utcb, unsigned cap_client_pseudonym, const char *name, long invalue, long *outvalue=0) {
    init_frame(utcb, TYPE_GET_QUOTA, CAP_PARENT_ID) << invalue << Utcb::String(name)
						    << Utcb::TypedIdentifyCap(Crd(cap_client_pseudonym, 0, DESC_CAP_ALL));
    unsigned res = call(utcb, CAP_PT_PERCPU, false);
    if (!res && outvalue && utcb >> *outvalue)  res = EPROTO;
    utcb.drop_frame();
    return res;
  }

  /// @see check_singleton()
  static unsigned set_singleton(Utcb &utcb, unsigned cap_client_pseudonym, unsigned cap_local_session) {
    init_frame(utcb, TYPE_SINGLETON, CAP_PARENT_ID) << 1U << Utcb::TypedIdentifyCap(Crd(cap_client_pseudonym, 0, DESC_CAP_ALL))
						    << Utcb::TypedMapCap(Crd(cap_local_session, 0, DESC_CAP_ALL));
    return call(utcb, CAP_PT_PERCPU, true);
  }

  /// Services can use check_singleton() in conjunction with
  /// set_singleton() to enforce a one-session-per-client policy. This is
  /// not done automatically.
  static unsigned check_singleton(Utcb &utcb, unsigned cap_client_pseudonym, unsigned &cap_local_session,
				  Crd crd = Crd(0, 31, DESC_CAP_ALL)) {
    init_frame(utcb, TYPE_SINGLETON, CAP_PARENT_ID) << 2U << Utcb::TypedIdentifyCap(Crd(cap_client_pseudonym, 0, DESC_CAP_ALL));
    utcb.head.crd_translate = crd.value();
    Utcb::Frame input(&utcb, sizeof(utcb.msg) / sizeof(unsigned));
    unsigned res = call(utcb, CAP_PT_PERCPU, false);
    cap_local_session = input.translated_cap().cap();
    utcb.drop_frame();
    return res;
  }
  static unsigned kill(Utcb &utcb, unsigned cap_client_pseudonym, unsigned service_cap = 0) {
    init_frame(utcb, TYPE_REQ_KILL, CAP_PARENT_ID) << Utcb::TypedIdentifyCap(Crd(cap_client_pseudonym, 0, DESC_CAP_ALL));
    return call(utcb, service_cap ? service_cap : 0U + CAP_PT_PERCPU, true, !service_cap);
  }

  /**
   * Signals an (arbitrary) event to the parent.
   * @param value Identifies the event.
   */
  static unsigned signal(Utcb &utcb, unsigned value) {
    init_frame(utcb, TYPE_SIGNAL, CAP_PARENT_ID) << value;
    return call(utcb, CAP_PT_PERCPU, true);
  }
};

/**
 * Generic protocol handling, hides the parent protocol
 * handling. Specific protocols will be inherited from it.
 * This is meant to be used only by clients.
 *
 * Features: session-open, parent-open, request-portal, optional blocking
 * Missing: restrict-quota
 */
class GenericProtocol : public ParentProtocol {
protected:
  const char *_service;
  unsigned    _instance;
  unsigned    _cap_base;	///< Base of the capability range. This cap refers to CAP_PSEUDONYM.
  unsigned    _session_base;    ///< Base of the session portals.
  Semaphore   _lock;
  bool        _blocking;
  bool        _disabled;
public:

  /// Client capabilities used to talk to the service 
  enum {
    CAP_PSEUDONYM,
    CAP_LOCK,
    CAP_SERVER_SESSION,
    CAP_SERVER_PT              ///< Portal for CPU0
  };

  /**
   * Call the server in a loop to resolve all faults.
   */
  unsigned call_server(Utcb &utcb, bool drop_frame) {
    unsigned res = EPERM;
    unsigned mtr = utcb.head.mtr;
    unsigned w0  = utcb.msg[0];
    while (!_disabled) {
    do_call:
      res = call(utcb, _session_base, false);
      if (_session_base == _cap_base + CAP_SERVER_PT) {
        if (res == EEXISTS) {
          utcb.head.mtr = mtr;
          utcb.msg[0]   = w0;
          goto do_open_session;
        }
        if (res == NOVA_ECAP) goto do_get_portal;
      } else { // Per session portals
        if (res == NOVA_ECAP) goto do_open_session;
      }
      goto do_out;

    do_open_session:
      cap_sel sess;
      if (_session_base == _cap_base + CAP_SERVER_PT)
        sess = _cap_base + CAP_SERVER_SESSION; // Clients are identified by a translated semaphore
      else
        sess = _session_base + utcb.head.nul_cpunr; // Clients are identified by session portals

      //if we have already a server session revoke it to be able to receive an new mapping (overmapping not supported!)
      //- possible reason: server is gone and a new one is now in place
      if (nova_lookup(Crd(sess, 0, DESC_CAP_ALL)).attr() & DESC_TYPE_CAP)
        nova_revoke(Crd(sess, 0, DESC_CAP_ALL), true);

      utcb.add_frame() << TYPE_OPEN << Utcb::TypedMapCap(Crd(_cap_base + CAP_PSEUDONYM, 0, DESC_CAP_ALL))
                       << Crd(sess, 0, DESC_CAP_ALL);
      res = call(utcb, _cap_base + CAP_SERVER_PT, true);

      if (res == NOVA_ECAP)  goto do_get_portal;
      if (res == EEXISTS)    goto do_get_pseudonym;
      if (res == ENONE) {
        _disabled = !(nova_lookup(Crd(sess, 0, DESC_CAP_ALL)).attr() & DESC_TYPE_CAP);
        //stop here if server try to cheat us and avoid so looping potentially endless
        if (_disabled) res = EPERM;
        else goto do_call;
      }
      goto do_out;
    do_get_portal:
      {
        // we lock to avoid missing wakeups on retry
        SemaphoreGuard guard(_lock);
        res = ParentProtocol::get_portal(utcb, _cap_base + CAP_PSEUDONYM,
					 _cap_base + CAP_SERVER_PT + utcb.head.nul_cpunr, _blocking, _service);
      }
      if (res == ERETRY && _blocking) goto do_get_portal;
      if (res == ENONE)     goto do_call;
      if (res == EEXISTS)   goto do_get_pseudonym;
      goto do_out;

    do_get_pseudonym:
      //if we have already a pseudonym revoke it to be able to receive an new mapping (overmapping not supported!)
      //- possible reason: server is gone and a new one is now in place
      if (nova_lookup(Crd(_cap_base + CAP_PSEUDONYM, 0, DESC_CAP_ALL)).attr() & DESC_TYPE_CAP)
        nova_revoke(Crd(_cap_base + CAP_PSEUDONYM, 0, DESC_CAP_ALL), true);

      res = ParentProtocol::get_pseudonym(utcb, _service, _instance, _cap_base + CAP_PSEUDONYM);
      if (res == ENONE) goto do_call;
      _disabled = true;
      goto do_out;
    }
  do_out:
    if (drop_frame) utcb.drop_frame();
    return res;
  }

  unsigned call_server_drop(Utcb &utcb) { return call_server(utcb, true); }
  unsigned call_server_keep(Utcb &utcb) { return call_server(utcb, false); }
  /**
   * Destroy the object.
   *
   * - Revoke all caps and add caps back to cap allocator.
   */
  template <class T>
  void destroy(Utcb &utcb, unsigned portal_num, T * obj) {
    release_pseudonym(utcb, _cap_base + CAP_PSEUDONYM);
    obj->dealloc_cap(_cap_base, portal_num);
  }

  // XXX: Clarify the intended difference between destroy() and close()
  /**
   * Close the session to the parent.
   * - Revoke all caps we got from external and we created (default: revoke_lock = true)
   * - If revoke_lock == false than this object can be reused afterwards to create another session.
   */
  void close(Utcb &utcb, unsigned portal_num, bool revoke_lock = true, bool _release_pseudonym = true) {
    unsigned char res;
    if (_release_pseudonym) release_pseudonym(utcb, _cap_base + CAP_PSEUDONYM);

    if (revoke_lock) res = nova_revoke(Crd(_cap_base + CAP_LOCK, 0, DESC_CAP_ALL), true);
    res = nova_revoke(Crd(_cap_base + CAP_PSEUDONYM, 0, DESC_CAP_ALL), true);
    res = nova_revoke(Crd(_cap_base + CAP_SERVER_SESSION, 0, DESC_CAP_ALL), true);
    if (portal_num > CAP_SERVER_PT)
      for (unsigned i=0; i < portal_num - CAP_SERVER_PT; i++) {
        res = nova_revoke(Crd(_cap_base + CAP_SERVER_PT + i, 0, DESC_CAP_ALL), true);
        if (_session_base != _cap_base + CAP_SERVER_PT)
          res = nova_revoke(Crd(_session_base + i, 0, DESC_CAP_ALL), true);
      }
    (void)res;
  }

  unsigned get_notify_sm() { return _cap_base + CAP_SERVER_SESSION; }

  static Utcb & init_frame_noid(Utcb &utcb, unsigned op) { return utcb.add_frame() << op; }
  Utcb & init_frame(Utcb &utcb, unsigned op) {
    return utcb.add_frame() << op << Utcb::TypedIdentifyCap(Crd(_cap_base + CAP_SERVER_SESSION, 0, DESC_CAP_ALL)); }

  GenericProtocol(const char *service, unsigned instance, unsigned cap_base, bool blocking, unsigned session_base=~0u)
    : _service(service), _instance(instance), _cap_base(cap_base), _session_base(session_base == ~0u ? _cap_base + CAP_SERVER_PT : session_base),  _lock(cap_base + CAP_LOCK), _blocking(blocking), _disabled(false)
  {
    UNUSED unsigned res = nova_create_sm(cap_base + CAP_LOCK);
    assert(res == NOVA_ESUCCESS);
    _lock.up();
//    Logging::printf("New Protocol '%s' base %x\n", service, cap_base);
  }
};

/// Helper class that replaces calls to init_frame() with calls to init_frame_noid().
class GenericNoXlateProtocol : public GenericProtocol {
public:
  GenericNoXlateProtocol(const char *service, unsigned instance, unsigned cap_base, bool blocking, unsigned session_base=~0u)
    : GenericProtocol(service, instance, cap_base, blocking, session_base) {}

  Utcb & init_frame(Utcb &utcb, unsigned op) { return init_frame_noid(utcb, op); }
};
