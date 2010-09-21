/*
 * Generic service helper.
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
#include "nul/parent.h"

/**
 * Optimize the request of different resources and the rollback if one failes.
 */
template <typename T> class QuotaGuard {
  Utcb *       _utcb;
  unsigned     _parent_cap;
  const char * _name;
  long         _value_in;
  QuotaGuard * _prev;
  unsigned     _res;
 public:
  QuotaGuard(Utcb *utcb, unsigned parent_cap, const char *name, long value_in, QuotaGuard *prev=0)
    : _utcb(utcb), _parent_cap(parent_cap), _name(name), _value_in(value_in), _prev(prev), _res(ENONE) {
    _res = T::get_quota(_utcb, _parent_cap, _name, _value_in);
  }
  ~QuotaGuard()   { if (_value_in && !_res) T::get_quota(_utcb, _parent_cap, _name, _value_in); }
  void commit()   { for (QuotaGuard *g = this; g; g = g->_prev) g->_value_in = 0; }
  unsigned status() { for (QuotaGuard *g = this; g; g = g->_prev) if (g->_res) return g->_res; return _res; }
};


/**
 * Data that is stored by every client.
 */
struct GenericClientData {
  void              *next;
  unsigned           parent_cap;
  unsigned           identity;
  /**
   * We implement a get_quota here, so that derived classes can
   * overwrite it.  Usefull for example in sigma0 that has a different
   * way to get the quota of a client.
   */
  static unsigned get_quota(Utcb *utcb, unsigned _parent_cap, const char *name, long value_in, long *value_out=0) {
    return ParentProtocol::get_quota(utcb, _parent_cap, name, value_in, value_out);
  }

  /**
   * Close the session at the parent.  Implemented here so that
   * derived classes can overwrite it.
   */
  void session_close(Utcb *utcb) {
    ParentProtocol::session_close(utcb, parent_cap);
  }
};


/**
 * A generic container that stores per-client data.
 * Missing: dequeue-from-list, iterator
 */
template <typename T> class ClientDataStorage {
  T *_head;
public:
  unsigned alloc_client_data(Utcb *utcb, T *&data, unsigned parent_cap) {
    unsigned res;
    QuotaGuard<T> guard1(utcb, parent_cap, "mem", sizeof(T));
    QuotaGuard<T> guard2(utcb, parent_cap, "cap", 2, &guard1);
    check1(res, res = guard2.status());
    guard2.commit();

    data = new T;
    data->parent_cap = parent_cap;
    data->identity   = alloc_cap();
    nova_create_sm(data->identity);

    // enqueue it
    data->next = _head;
    _head = data;
    return ENONE;
  }


  unsigned free_client_data(Utcb *utcb, T *data) {
    for (T **prev = &_head; *prev; prev = reinterpret_cast<T **>(&(*prev)->next))
      if (*prev == data) {
	*prev = reinterpret_cast<T *>(data->next);
	data->next = 0;
	T::get_quota(utcb, data->parent_cap,"cap", -2);
	T::get_quota(utcb, data->parent_cap, "mem", -sizeof(T));
	data->session_close(utcb);
	dealloc_cap(data->identity);
	dealloc_cap(data->parent_cap);
	delete data;
	return ENONE;
      }
    assert(0);
  }


  unsigned get_client_data(Utcb *utcb, T *&data, unsigned identity) {
    for (T * client = next(); client && identity; client = next(client))
      if (client->identity == identity) {
	data = client;
	return ENONE;
      }
    Logging::printf("could not find client data for %x\n", identity);
    return EEXISTS;
  }

  /**
   * Iterator.
   */
  T* next(T *prev=0) { if (!prev) return _head;  return reinterpret_cast<T *>(prev->next); }
};


/**
 * Define a static portal function.
 */
template <class C> struct StaticPortalFunc {
  static unsigned long portal_func(C *tls, Utcb *utcb) __attribute__((regparm(0)))
  {
    bool free_cap = utcb->get_received_cap();
    bool need_alloc = free_cap;
    Mtd mtr = utcb->head.mtr;
    utcb->add_frame().head.mtr.add_untyped();
    utcb->msg[0] = tls->portal_func(utcb, free_cap, mtr);
    unsigned res = utcb->skip_frame();
    if (free_cap)
      nova_revoke(Crd(utcb->get_received_cap(), 0, DESC_CAP_ALL), true);
    else if (need_alloc)
      utcb->head.crd = alloc_cap() << Utcb::MINSHIFT | DESC_TYPE_CAP;
    return res;
  }
};
