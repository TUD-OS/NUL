/*
 * Parent protocol.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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
 * Missing:   restrict quota, get_quota
 */
struct ParentProtocol {

  enum {
    // generic protocol functions
    TYPE_OPEN,
    TYPE_CLOSE,
    TYPE_GENERIC_END,
    // server specific functions
    TYPE_REQUEST,
    TYPE_REGISTER,
    TYPE_UNREGISTER,
    TYPE_GET_QUOTA,
    CAP_PT_PERCPU = 256,
  };

  static unsigned call(Utcb *utcb, unsigned cap_base) {
    unsigned res;
    res = nova_call(cap_base + Cpu::cpunr(), utcb->head.mtr_out);
    return res ? res : utcb->msg[0];
  }

  static unsigned session_open(Utcb *utcb, const char *service, unsigned cap_parent_session) __attribute__((noinline)) {
    utcb->init_frame() << TYPE_OPEN << service << Crd(cap_parent_session, 0, DESC_CAP_ALL);
    return call(utcb, CAP_PT_PERCPU);
  }


  static unsigned request_portal(Utcb *utcb, unsigned cap_parent_session, unsigned cap_portal, bool blocking) {
    utcb->init_frame() << TYPE_REQUEST << Utcb::TypedIdentifyCap(cap_parent_session) << Crd(cap_portal, 0, DESC_CAP_ALL);
    unsigned res = call(utcb, CAP_PT_PERCPU);

    // block on the parent session until something happens
    if (res == ERETRY) {
      Logging::printf("block waiting for session %x\n", cap_parent_session);
      if (blocking)
	nova_semdownmulti(cap_parent_session);
      else res = EABORT;
    }
    return res;
  }


  static unsigned session_close(Utcb *utcb, unsigned cap_parent_session) {
    utcb->init_frame() << TYPE_CLOSE << Utcb::TypedIdentifyCap(cap_parent_session);
    return call(utcb, CAP_PT_PERCPU);
  }


  static unsigned register_service(Utcb *utcb, const char *service, unsigned cpu, unsigned pt, unsigned cap_service) {
    utcb->init_frame() << TYPE_REGISTER << cpu << service << Utcb::TypedMapCap(pt) << Crd(cap_service, 0, DESC_CAP_ALL);
    return call(utcb, CAP_PT_PERCPU);
  };


  static unsigned unregister_service(Utcb *utcb, unsigned cap_service) {
    utcb->init_frame() << TYPE_UNREGISTER << Utcb::TypedIdentifyCap(cap_service);
    return call(utcb, CAP_PT_PERCPU);
  }


  static unsigned get_quota(Utcb *utcb, unsigned parent_cap, const char *name, long invalue, long *outvalue=0) {
    utcb->init_frame() << TYPE_GET_QUOTA << invalue << name << Utcb::TypedIdentifyCap(parent_cap);
    unsigned res = call(utcb, CAP_PT_PERCPU);
    if (!res && outvalue) {
      if (utcb->head.mtr.untyped() < 2) return EPROTO;
      *outvalue = utcb->msg[1];
    }
    return res;
  }

};


/**
 * Generic protocol handling, hides the parent protocol
 * handling. Specific protocols will be inherited from it.
 *
 * Features: session-open, parent-open, request-portal, optional blocking
 * Missing: restrict-quota
 */
class GenericProtocol : public ParentProtocol {
protected:
  const char *_service;
  unsigned    _cap_base;
  Semaphore   _lock;
  bool        _blocking;
  bool        _disabled;
public:

  enum {
    CAP_PARENT_SESSION,
    CAP_LOCK,
    CAP_SERVER_SESSION,
    CAP_SERVER_PT,
    CAP_NUM = CAP_SERVER_PT + Config::MAX_CPUS,
  };

  /**
   * Call a server and handles the parent and session errors after the call.
   */
  unsigned do_call(Utcb *utcb) {
    unsigned res = call(utcb, _cap_base + CAP_SERVER_PT);
    Logging::printf("%s %x\n", __func__, res);
    bool need_session = res == EEXISTS;
    bool need_pt      = res == NOVA_ECAP;
    bool need_open = false;
    if (need_session) {
      utcb->init_frame() << TYPE_OPEN << Utcb::TypedMapCap(_cap_base + CAP_PARENT_SESSION) << Crd(_cap_base + CAP_SERVER_SESSION, 0, DESC_CAP_ALL);
      res = call(utcb, _cap_base + CAP_SERVER_PT);
      if (!res)  return ERETRY;
      need_pt      = res == NOVA_ECAP;
      need_open = res == EEXISTS;
    }
    if (need_pt) {
      Logging::printf("request portal for %s\n", _service);
      {
	// we lock to avoid missing wakeups on retry
	SemaphoreGuard guard(_lock);
	res = ParentProtocol::request_portal(utcb, _cap_base + CAP_PARENT_SESSION, _cap_base + CAP_SERVER_PT + Cpu::cpunr(), _blocking);
      }
      Logging::printf("request portal for %s returned %x\n", _service, res);
      if (!res)  return ERETRY;
      need_open = res == EEXISTS;
    }
    if (need_open) {
      res = ParentProtocol::session_open(utcb, _service, _cap_base + CAP_PARENT_SESSION);
      if (!res)  return ERETRY;
      _disabled = res == EPERM || res == EEXISTS;
    }
    return res;
  }


  void close(Utcb *utcb) {
    session_close(utcb, _cap_base + CAP_PARENT_SESSION);
  }


  GenericProtocol(const char *service, unsigned cap_base, bool blocking)
    : _service(service), _cap_base(cap_base), _lock(cap_base + CAP_LOCK), _blocking(blocking), _disabled(false)
  {
    nova_create_sm(cap_base + CAP_LOCK);
    _lock.up();
    Logging::printf("New Protocol '%s' base %x\n", service, cap_base);
  }
};


/**
 * Client part of the log protocol.
 *
 * Missing: handle very-long strings, disable on EPERM
 */
struct LogProtocol : public GenericProtocol {
  enum {
    TYPE_LOG = ParentProtocol::TYPE_GENERIC_END,
  };
  unsigned log(Utcb *utcb, const char *line) {
    if (_disabled) return EPERM;
    unsigned res;
    do {
      utcb->init_frame() << TYPE_LOG << line << Utcb::TypedIdentifyCap(_cap_base + CAP_SERVER_SESSION);
      res = do_call(utcb);
    } while (res == ERETRY);
    return res;
  }

 LogProtocol(unsigned cap_base) : GenericProtocol("log", cap_base, true) {}
};
