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
    TYPE_OPEN,
    TYPE_CLOSE,
    TYPE_REQUEST,
    TYPE_REGISTER,
    TYPE_UNREGISTER,
    CAP_PT_PERCPU = 256,
  };

  static unsigned call(Utcb *utcb, unsigned cap_base) {
    unsigned res;
    res = nova_call(cap_base + Cpu::cpunr(), utcb->head.mtr);
    return res ? res : utcb->msg[0];
  }
  static void init_utcb(Utcb *utcb, unsigned words, unsigned cap_id=0, unsigned cap_receive=0, bool map_id=false) {
    utcb->head.mtr = Mtd(words, cap_id ? 1 : 0);
    assert (utcb->head.mtr.untyped() + 2*utcb->head.mtr.typed() <= (sizeof(Utcb) - Utcb::HEADER_SIZE));
    utcb->head.crd = cap_receive ? DESC_TYPE_CAP | cap_receive << Utcb::MINSHIFT : 0;
    if (cap_id) {
      utcb->msg[words + 0] = DESC_CAP_ALL | cap_id << Utcb::MINSHIFT;
      utcb->msg[words + 1] = map_id ? 1 : 0;
    }
  }



  static unsigned session_open(Utcb *utcb, const char *service, unsigned cap_parent_session){
    unsigned slen =  strlen(service) + 1;
    init_utcb(utcb, 1 + (slen + sizeof(unsigned) - 1) / sizeof(unsigned), 0, cap_parent_session);
    utcb->msg[utcb->head.mtr.untyped() - 1] = 0;
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
    init_utcb(utcb, 2 + (slen + sizeof(unsigned) - 1) / sizeof(unsigned), pt, cap_service, true);
    utcb->msg[0] = TYPE_REGISTER;
    utcb->msg[1] = cpu;
    utcb->msg[utcb->head.mtr.untyped()-1] = 0;
    memcpy(utcb->msg + 2, service, slen);
    return call(utcb, CAP_PT_PERCPU);
  };


  static unsigned unregister_service(Utcb *utcb, unsigned cap_service) {
    init_utcb(utcb, 1, cap_service);
    utcb->msg[0] = TYPE_UNREGISTER;
    return call(utcb, CAP_PT_PERCPU);
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
      init_utcb(utcb, 0, _cap_base + CAP_PARENT_SESSION, _cap_base + CAP_SERVER_SESSION, true);
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
  unsigned log(Utcb *utcb, const char *line) {
    if (_disabled) return EPERM;
    unsigned res;
    do {
      unsigned slen = strlen(line) + 1;
      init_utcb(utcb, (slen + sizeof(unsigned) - 1) / sizeof(unsigned), _cap_base + CAP_SERVER_SESSION);
      utcb->msg[utcb->head.mtr.untyped()-1] = 0;
      memcpy(utcb->msg, line, slen);
      res = do_call(utcb);
    } while (res == ERETRY);
    return res;
  }

 LogProtocol(unsigned cap_base) : GenericProtocol("log", cap_base, true) {}
};
