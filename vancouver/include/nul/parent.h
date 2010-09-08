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
  static void init_utcb(Utcb *utcb, unsigned words, unsigned cap_id=0, unsigned cap_receive=0, unsigned cap_map=0) {
    assert (words + 4 <= (sizeof(Utcb) - Utcb::HEADER_SIZE));
    utcb->head.crd = cap_receive ? DESC_TYPE_CAP | cap_receive << Utcb::MINSHIFT : 0;
    unsigned *msg = utcb->msg + words;
    if (cap_id) {
      *msg++ = DESC_CAP_ALL | cap_id << Utcb::MINSHIFT;
      *msg++ = 0;
    }
    if (cap_map) {
      *msg++ = DESC_CAP_ALL | cap_map << Utcb::MINSHIFT;
      *msg++ = 1;
    }
    utcb->head.mtr_out = Mtd(words, ((msg - utcb->msg) - words) / 2);
  }


  static unsigned session_open(Utcb *utcb, const char *service, unsigned cap_parent_session){
    unsigned slen =  strlen(service) + 1;
    init_utcb(utcb, 1 + (slen + sizeof(unsigned) - 1) / sizeof(unsigned), 0, cap_parent_session);
    utcb->msg[utcb->head.mtr_out.untyped() - 1] = 0;
    memcpy(utcb->msg + 1, service, slen);
    utcb->msg[0] = TYPE_OPEN;
    return call(utcb, CAP_PT_PERCPU);
  }


  static unsigned request_portal(Utcb *utcb, unsigned cap_parent_session, unsigned cap_portal, bool blocking) {
    init_utcb(utcb, 1, cap_parent_session, cap_portal);
    utcb->msg[0] = TYPE_REQUEST;
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
    init_utcb(utcb, 1, cap_parent_session);
    utcb->msg[0] = TYPE_CLOSE;
    return call(utcb, CAP_PT_PERCPU);
  }


  static unsigned register_service(Utcb *utcb, const char *service, unsigned cpu, unsigned pt, unsigned cap_service) {
    unsigned slen =  strlen(service) + 1;
    init_utcb(utcb, 2 + (slen + sizeof(unsigned) - 1) / sizeof(unsigned), 0, cap_service, pt);
    utcb->msg[0] = TYPE_REGISTER;
    utcb->msg[1] = cpu;
    utcb->msg[utcb->head.mtr_out.untyped()-1] = 0;
    memcpy(utcb->msg + 2, service, slen);
    return call(utcb, CAP_PT_PERCPU);
  };


  static unsigned unregister_service(Utcb *utcb, unsigned cap_service) {
    init_utcb(utcb, 1, cap_service);
    utcb->msg[0] = TYPE_UNREGISTER;
    return call(utcb, CAP_PT_PERCPU);
  }


  static unsigned get_quota(Utcb *utcb, unsigned parent_cap, const char *name, long invalue, long *outvalue=0) {
    unsigned slen =  strlen(name) + 1;
    init_utcb(utcb, 2 + (slen + sizeof(unsigned) - 1) / sizeof(unsigned), parent_cap);
    utcb->msg[0] = TYPE_GET_QUOTA;
    utcb->msg[1] = invalue;
    utcb->msg[utcb->head.mtr_out.untyped()-1] = 0;
    memcpy(utcb->msg + 2, name, slen);
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
    bool need_session = res == EEXISTS;
    bool need_pt      = res == NOVA_ECAP;
    bool need_open = false;
    if (need_session) {
      init_utcb(utcb, 1, 0, _cap_base + CAP_SERVER_SESSION, _cap_base + CAP_PARENT_SESSION);
      utcb->msg[0] = TYPE_OPEN;
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
      _disabled = res == EPERM;
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
      unsigned slen = strlen(line) + 1;
      init_utcb(utcb, 1 + (slen + sizeof(unsigned) - 1) / sizeof(unsigned), _cap_base + CAP_SERVER_SESSION);
      utcb->msg[0] = TYPE_LOG;
      utcb->msg[utcb->head.mtr_out.untyped() - 1] = 0;
      memcpy(utcb->msg + 1, line, slen);
      res = do_call(utcb);
    } while (res == ERETRY);
    return res;
  }

 LogProtocol(unsigned cap_base) : GenericProtocol("log", cap_base, true) {}
};
