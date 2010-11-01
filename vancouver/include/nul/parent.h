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
    TYPE_INVALID = 0, // used as error indicator
    TYPE_OPEN,
    TYPE_CLOSE,
    TYPE_GENERIC_END,

    // server specific functions
    TYPE_REQUEST,
    TYPE_REGISTER,
    TYPE_UNREGISTER,
    TYPE_GET_QUOTA,
    CAP_PARENT_ID = 255,
    CAP_PT_PERCPU = 256,
  };

  static Utcb & init_frame(Utcb &utcb, unsigned op, unsigned id) { return utcb.add_frame() << op << Utcb::TypedIdentifyCap(id); }

  /**
   * Low-level systemcall.
   */
  static unsigned call(Utcb &utcb, unsigned cap_base, bool drop_frame) {
    unsigned res;
    res = nova_call(cap_base + Cpu::cpunr());
    if (!res) res = utcb.msg[0];
    if (drop_frame) utcb.drop_frame();
    return res;
  }

  static unsigned get_pseudonym(Utcb &utcb, const char *service, unsigned instance, unsigned cap_parent_session) {
    return call(init_frame(utcb, TYPE_OPEN, CAP_PARENT_ID) << instance << service << Crd(cap_parent_session, 0, DESC_CAP_ALL), CAP_PT_PERCPU, true);
  }

  static unsigned release_pseudonym(Utcb &utcb, unsigned cap_parent_session) {
    init_frame(utcb, TYPE_CLOSE, CAP_PARENT_ID) << Utcb::TypedIdentifyCap(cap_parent_session);
    return call(utcb, CAP_PT_PERCPU, true);
  }

  static unsigned request_portal(Utcb &utcb, unsigned cap_parent_session, unsigned cap_portal, bool blocking) {
    init_frame(utcb, TYPE_REQUEST, CAP_PARENT_ID) << Utcb::TypedIdentifyCap(cap_parent_session) << Crd(cap_portal, 0, DESC_CAP_ALL);
    unsigned res = call(utcb, CAP_PT_PERCPU, true);

    // block on the parent session until something happens
    if (res == ERETRY) {
      Logging::printf("block waiting for session %x\n", cap_parent_session);
      if (blocking)
	nova_semdownmulti(cap_parent_session);
      else res = EABORT;
    }
    return res;
  }

  static unsigned register_service(Utcb &utcb, const char *service, unsigned cpu, unsigned pt, unsigned cap_service) {
    assert(cap_service);
    init_frame(utcb, TYPE_REGISTER, CAP_PARENT_ID) << cpu << service << Utcb::TypedMapCap(pt) << Crd(cap_service, 0, DESC_CAP_ALL);
    return call(utcb, CAP_PT_PERCPU, true);
  };

  static unsigned unregister_service(Utcb &utcb, unsigned cap_service) {
    init_frame(utcb, TYPE_UNREGISTER, CAP_PARENT_ID) << Utcb::TypedIdentifyCap(cap_service);
    return call(utcb, CAP_PT_PERCPU, true);
  }

  static unsigned get_quota(Utcb &utcb, unsigned parent_cap, const char *name, long invalue, long *outvalue=0) {
    init_frame(utcb, TYPE_GET_QUOTA, CAP_PARENT_ID) << invalue << name << Utcb::TypedIdentifyCap(parent_cap);
    unsigned res = call(utcb, CAP_PT_PERCPU, false);
    if (!res && outvalue && utcb >> *outvalue)  res = EPROTO;
    utcb.drop_frame();
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
  unsigned    _instance;
  unsigned    _cap_base;
  Semaphore   _lock;
  bool        _blocking;
  bool        _disabled;
public:

  enum {
    CAP_PSEUDONYM,
    CAP_LOCK,
    CAP_SERVER_SESSION,
    CAP_SERVER_PT,
    CAP_NUM = CAP_SERVER_PT + Config::MAX_CPUS,
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
      res = call(utcb, _cap_base + CAP_SERVER_PT, false);
      if (res == EEXISTS) {
        utcb.head.mtr = mtr;
        utcb.msg[0]   = w0;
        goto do_open_session;
      }
      if (res == NOVA_ECAP) goto do_request_portal;
      goto do_out;

    do_open_session:
      utcb.add_frame() << TYPE_OPEN << Utcb::TypedMapCap(_cap_base + CAP_PSEUDONYM) << Crd(_cap_base + CAP_SERVER_SESSION, 0, DESC_CAP_ALL);
      res = call(utcb, _cap_base + CAP_SERVER_PT, true);
      if (res == ENONE)     goto do_call;
      if (res == EEXISTS)   goto do_get_pseudonym;
      if (res == NOVA_ECAP) goto do_request_portal;
      goto do_out;
    do_request_portal:
      {
	// we lock to avoid missing wakeups on retry
	SemaphoreGuard guard(_lock);
	res = ParentProtocol::request_portal(utcb, _cap_base + CAP_PSEUDONYM, _cap_base + CAP_SERVER_PT + Cpu::cpunr(), _blocking);
      }
      if (res == ENONE)     goto do_call;
      if (res == EEXISTS)   goto do_get_pseudonym;
      goto do_out;

    do_get_pseudonym:
      res = ParentProtocol::get_pseudonym(utcb, _service, _instance, _cap_base + CAP_PSEUDONYM);
      if (res == ENONE)     goto do_call;
      _disabled = true;
      goto do_out;
    }
  do_out:
    if (drop_frame) utcb.drop_frame();
    return res;
  }

  /**
   * Close the session.
   */
  void close(Utcb &utcb) { release_pseudonym(utcb, _cap_base + CAP_PSEUDONYM); }


  Utcb & init_frame(Utcb &utcb, unsigned op) { return utcb.add_frame() << op << Utcb::TypedIdentifyCap(_cap_base + CAP_SERVER_SESSION); }


  GenericProtocol(const char *service, unsigned instance, unsigned cap_base, bool blocking)
    : _service(service), _instance(instance), _cap_base(cap_base), _lock(cap_base + CAP_LOCK), _blocking(blocking), _disabled(false)
  {
    nova_create_sm(cap_base + CAP_LOCK);
    _lock.up();
    Logging::printf("New Protocol '%s' base %x\n", service, cap_base);
  }
};


/**
 * Client part of the log protocol.
 *
 * Missing: handle very-long strings
 */
struct LogProtocol : public GenericProtocol {
  enum {
    TYPE_LOG = ParentProtocol::TYPE_GENERIC_END,
  };
  unsigned log(Utcb &utcb, const char *line) {
    return call_server(init_frame(utcb, TYPE_LOG) << line, true);
  }

  LogProtocol(unsigned cap_base, unsigned instance=0) : GenericProtocol("log", instance, cap_base, true) {}
};


#include "host/dma.h"


/**
 * Client part of the disk protocol.
 * Missing: register shared memory producer/consumer.
 */
struct DiscProtocol : public GenericProtocol {
  enum {
    TYPE_GET_PARAMS = ParentProtocol::TYPE_GENERIC_END,
    TYPE_READ,
    TYPE_WRITE,
    TYPE_FLUSH_CACHE,
    TYPE_GET_COMPLETION,
  };

  unsigned get_params(Utcb &utcb, DiskParameter *params) {
    unsigned res;
    if (!(res = call_server(init_frame(utcb, TYPE_GET_PARAMS), false)))
      if (utcb >> *params)  res = EPROTO;
    utcb.drop_frame();
    return res;
  }


  unsigned read_write(Utcb &utcb, bool read, unsigned long usertag, unsigned long long sector,
		unsigned long physoffset, unsigned long physsize,
		unsigned dmacount, DmaDescriptor *dma)
  {
    init_frame(utcb, read ? TYPE_READ : TYPE_WRITE) << usertag << sector << physoffset << physsize << dmacount;
    for (unsigned i=0; i < dmacount; i++)  utcb << dma[i];
    return call_server(init_frame(utcb, TYPE_GET_PARAMS), true);
  }



  unsigned flush_cache(Utcb &utcb) {
    return call_server(init_frame(utcb, TYPE_FLUSH_CACHE), true);
  }


  unsigned get_completion(Utcb &utcb, unsigned &tag, unsigned &status) {
    unsigned res;
    if (!(res = call_server(init_frame(utcb, TYPE_GET_COMPLETION), false)))
      if (utcb >> tag || utcb >> status)  res = EPROTO;
    utcb.drop_frame();
    return res;
  }

  DiscProtocol(unsigned cap_base, unsigned disknr) : GenericProtocol("disk", disknr, cap_base, true) {}
};
