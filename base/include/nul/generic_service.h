/** @file
 * Generic service helper.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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
#include "nul/parent.h"
#include "nul/baseprogram.h"

/**
 * Optimize the request of different resources and the rollback if one failes.
 */
template <class T> class QuotaGuard {
  Utcb &       _utcb;
  unsigned     _pseudonym;
  const char * _name;
  long         _value_in;
  QuotaGuard * _prev;
  unsigned     _res;
 public:
  QuotaGuard(Utcb &utcb, unsigned pseudonym, const char *name, long value_in, QuotaGuard *prev=0)
    : _utcb(utcb), _pseudonym(pseudonym), _name(name), _value_in(value_in), _prev(prev), _res(ENONE) {
    _res = T::get_quota(_utcb, _pseudonym, _name, _value_in);
  }
  ~QuotaGuard()   { if (_value_in && !_res) T::get_quota(_utcb, _pseudonym, _name, _value_in); }
  void commit()   { for (QuotaGuard *g = this; g; g = g->_prev) g->_value_in = 0; }
  unsigned status() { for (QuotaGuard *g = this; g; g = g->_prev) if (g->_res) return g->_res; return _res; }
};


/**
 * Data that is stored by a service for every client.
 */
struct BaseClientData {
  void * next;
  void * del;
  unsigned           pseudonym; ///< A capability identifying the client. This is also known to the parent.
  /**
   * We implement a get_quota here, so that derived classes can
   * overwrite it.  Useful for example in sigma0 that has a different
   * way to get the quota of a client.
   */
  static unsigned get_quota(Utcb &utcb, unsigned _pseudonym, const char *name, long value_in, long *value_out=0) {
    return ParentProtocol::get_quota(utcb, _pseudonym, name, value_in, value_out);
  }

  /**
   * Close the session at the parent.  Implemented here so that
   * derived classes can overwrite it.
   */
  void session_close(Utcb &utcb) {
    ParentProtocol::release_pseudonym(utcb, pseudonym);
  }
};

class GenericClientData : public BaseClientData {
  cap_sel identity;  ///< A capability created by the service to identify the session.
public:
  void    set_identity(cap_sel id)   { identity = id; }
  cap_sel get_identity()             { return identity; }
  void    set_singleton(cap_sel cap) { }
  cap_sel get_singleton()            { return identity; }

  template <class A>
  void dealloc_identity(A * obj) { obj->dealloc_cap(identity); }
};

// ClientData base type for use with NoXlateSService
class PerCpuIdClientData : public BaseClientData {
  cap_sel portals[Config::MAX_CPUS];
  cap_sel singleton;
public:
  void    set_identity(cap_sel id)   { portals[BaseProgram::myutcb()->head.nul_cpunr] = id; }
  cap_sel get_identity()             { return portals[BaseProgram::myutcb()->head.nul_cpunr]; }
  void    set_singleton(cap_sel cap) { singleton = cap; }
  cap_sel get_singleton()            { return singleton; }

  template <class A>
  void dealloc_identity(A * obj) {
    for (unsigned i = 0; i < Config::MAX_CPUS; i++) obj->dealloc_cap(portals[i]);
  }
};

/**
 * A generic container that stores per-client data.
 * Missing: iterator
 */
template <class T, class A, bool free_pseudonym = true, bool __DEBUG__ = false>
class ClientDataStorage {
  struct recycl {
    T * head;
    unsigned long t_in;
  };
  struct recycl_nv {
    T * head;
    unsigned long t_in;
  };

  T * _head;
  union {
    ALIGNED(8) struct recycl recycling;
    ALIGNED(8) unsigned long long recyc64;
  };

  /**
   * Garbage collect - remove clients which are marked for removal
   */
  void gc(Utcb &utcb, A * obj) {
    unsigned long long tmp = recyc64;
    struct recycl tmp_expect = *reinterpret_cast<struct recycl *>(&tmp);
    if (tmp_expect.head && tmp_expect.t_in == 0) {
      unsigned long long tmp_read;
      tmp_read = Cpu::cmpxchg8b(&recyc64, *reinterpret_cast<unsigned long long *>(&tmp_expect), 0);
      if (!memcmp(&tmp_read, &tmp_expect, sizeof(tmp_read))) {
        unsigned err, crdout;
        unsigned long counter = 0;
        T * data, * tmp;
        struct recycl_nv nv_tmp;

        memcpy(&nv_tmp, &tmp_expect, sizeof(nv_tmp));
        data = nv_tmp.head;
        while(data) {
          err = nova_syscall(NOVA_LOOKUP,  Crd(data->pseudonym, 0, DESC_CAP_ALL).value(), 0, 0, 0, &crdout);
          // XXX Can this fail?
          (void)err;
          if (__DEBUG__) Logging::printf("gs: deleting %p identity=%#x pseudonym=%#x\n", data, data->get_identity(), data->pseudonym);
          if (crdout) {
            T::get_quota(utcb, data->pseudonym,"cap", -2);
            T::get_quota(utcb, data->pseudonym, "mem", -sizeof(T));
            data->session_close(utcb);
          }
          data->dealloc_identity(obj);
          if (free_pseudonym) obj->dealloc_cap(data->pseudonym);
          //tmp = data->del;
          unsigned long nv_del = reinterpret_cast<unsigned long>(&data->del);
          tmp = *reinterpret_cast<T **>(nv_del);
          delete data;
          data = tmp;
          counter ++;
        }
        if (__DEBUG__) Logging::printf("gs: cleaned objects %ld\n", counter);
      } else
        if (__DEBUG__) Logging::printf("gs: did not get cleaning list\n");
    }
  }

  unsigned create_identity(GenericClientData * data, A * obj) {
    return nova_create_sm(data->get_identity());
  }

  // XXX: We use client type here to distinguish between Xlate and
  // NoXlate services. This might not be always correct. For example,
  // it would be fine to use GenericClientData with NoXlate protocol
  // provided that the service doesn't enforce one session per client
  // policy (get/set_singleton()). This implementation would not allow
  // it.
  //
  // We could use type traits to select which implementation we want
  // here. This would also allow us to get rid of __DEBUG__ and
  // free_pseudonym template parameters which usually only clutter
  // compiler error messages.
  unsigned create_identity(PerCpuIdClientData * data, A * obj) {
    return obj->create_session_portal(data->get_identity());
  }

public:
  typedef T ClientData;
  typedef A CapAllocator;

  class Guard {
    ClientDataStorage * storage;
    A * obj;
    Utcb * utcb;

    public:

      /*
       * This guard triggers cleanup run on creation and deletion of this object
       */
      Guard(ClientDataStorage<T, A, free_pseudonym, __DEBUG__> * _storage, Utcb &_utcb, A * _obj) :
        storage(_storage), obj(_obj), utcb(&_utcb)
      {
        if (obj) storage->gc(*utcb, obj);
        Cpu::atomic_xadd(&storage->recycling.t_in, 1U);
      }

      /*
       * This guard don't trigger cleanup runs - use this for short running operation 
       */
      Guard(ClientDataStorage<T, A, free_pseudonym, __DEBUG__> * _storage) :
        storage(_storage), obj(0), utcb(0)
      {
        Cpu::atomic_xadd(&storage->recycling.t_in, 1U);
      }

      ~Guard() {
        Cpu::atomic_xadd(&storage->recycling.t_in, -1U);
        if (obj) storage->gc(*utcb, obj);
      }
  };

  ClientDataStorage() : _head(0), recyc64(0) {
    assert(sizeof(T *) == 4);
    assert(!(reinterpret_cast<unsigned long>(&recyc64) & 0x7));
  }

  unsigned alloc_identity(T * data, A * obj)
  {
    unsigned res, identity;
    identity = obj->alloc_cap();
    if (!identity) return ERESOURCE;
    data->set_identity(identity);
    data->set_singleton(identity);
    res = create_identity(data, obj);
    if (res) obj->dealloc_cap(identity);
    return res;
  }

  /**
   * Allocates session data for a client.
   *
   * Sets data->pseudonym to @a pseudonym and creates a new session
   * identity.
   *
   * @param utcb UTCB
   * @param[out] data Allocated client data
   * @param pseudonym Pseudonym that the client used to identify to us
   * @param obj CapAllocator
   *
   * @return
   */
  unsigned alloc_client_data(Utcb &utcb, T *&data, unsigned pseudonym, A * obj) {
    unsigned res;
    QuotaGuard<T> guard1(utcb, pseudonym, "mem", sizeof(T));
    QuotaGuard<T> guard2(utcb, pseudonym, "cap", 2, &guard1);
    check1(res, res = guard2.status());
    guard2.commit();

    data = new T;
    if (!data) return ERESOURCE;

    memset(data, 0, sizeof(T));
    data->pseudonym = pseudonym;
    res = alloc_identity(data, obj);
    if (res != ENONE) {
      delete data;
      data = 0;
      return res;
    }

    // enqueue it
    T * __head;
    do {
      __head = _head;
      data->next = __head;
    } while (reinterpret_cast<unsigned long>(__head) != Cpu::cmpxchg4b(&_head, reinterpret_cast<unsigned long>(__head), reinterpret_cast<unsigned long>(data)));
    if (__DEBUG__) Logging::printf("gs: alloc_client_data %p\n", data);
    return ENONE;
  }

  unsigned free_client_data(Utcb &utcb, T *data, A * obj) {
    Guard count(this, utcb, obj);

    for (T **prev = &_head; T *current = *prev; prev = reinterpret_cast<T**>(&current->next))
      if (current == data) {

        again:

        void * data_next = reinterpret_cast<volatile T*>(data)->next;
        if (reinterpret_cast<unsigned long>(data) != Cpu::cmpxchg4b(prev, reinterpret_cast<unsigned long>(data), reinterpret_cast<unsigned long>(data_next))) {
          if (__DEBUG__) Logging::printf("gs: another thread concurrently removed same data item - nothing todo\n");
          return 0;
        }

        if (reinterpret_cast<volatile T*>(data)->next != data_next) {
          // somebody dequeued our direct next in between ...
          //
          // Example: A and B dequeued in parallel
          //   prev = Y->next, data = A, data_next = B
          //
          //   X -> Y -> A -> B -> C -> D
          //
          //             A ------> C -> D
          //   X -> Y ------> B -> C -> D

          // AAA is performance optimization - AAA can also be removed. Issues caused by AAA can also be handled by BBB.
          // (AAA avoid searching for 'data' again.)
          
          // AAA start
          retry:

          void * tmp2_next = reinterpret_cast<volatile T*>(data)->next;
          void * got_next = reinterpret_cast<void *>(Cpu::cmpxchg4b(prev, reinterpret_cast<unsigned long>(data_next), reinterpret_cast<unsigned long>(tmp2_next)));
          if (got_next == data_next) {
            //ok - we managed to update the bogus pointer
            //
            // Example:
            //   prev = Y->next, data = A, data_next = B, tmp2_next = C
            //
            //             A ------> C -> D
            //   X -> Y ------> B -> C -> D
            //
            //             A ------> C -> D
            //                  B -> C -> D
            //   X -> Y -----------> C -> D
            if (reinterpret_cast<volatile T*>(data)->next != tmp2_next) {
              // Is update still valid or we updated the bogus pointer by a pointer which became bogus ?
              //             A -----------> D
              //   X -> Y -----------> C -> D
              data_next = tmp2_next;
              if (__DEBUG__) Logging::printf("gs: bogus pointer update - retry loop\n");
              goto retry; //if yes - we retry to update
            }
          } else {
            // ok - somebody else removed the bogus pointer for us
            //
            // Example:
            //             A ------> C -> D
            //   X -> Y ------> B -> C -> D
            //
            //             A ------> C -> D
            //                  B -> C -> D
            //   X -> Y ----------------> D
            if (__DEBUG__) Logging::printf("gs: bogus pointer removed by someone else\n");
          }
          //AAA end
          if (__DEBUG__) Logging::printf("gs: dequeue failed - fixup: success %p\n", data);
        }// else if (__DEBUG__) Logging::printf("gs: dequeue succeeded\n");

        //check that we are not in list anymore
        //BBB start
        for (prev = &_head; current = *prev; prev = reinterpret_cast<T**>(&current->next))
          if (current == data) {
            if (__DEBUG__) Logging::printf("gs: still in list - again loop %p\n", data);
            goto again;
          }
        //BBB end

        T * tmp;
        do {
          tmp = recycling.head;
          data->del = tmp;
        } while (reinterpret_cast<unsigned long>(tmp) != Cpu::cmpxchg4b(&recycling.head, reinterpret_cast<unsigned long>(tmp), reinterpret_cast<unsigned long>(data)));

        return 0;
      }

    if (__DEBUG__) {
      Logging::printf("didn't find item %p\n list:", data);
    
      for (T ** prev = &_head; T *current = *prev; prev = reinterpret_cast<T **>(&current->next))
        Logging::printf(" %p ->", current);
    }
    __builtin_trap();
  }

  /**
   * Iterator.
   */
  T * next(T *prev = 0) {
    assert(recycling.t_in);
    if (!prev) return _head;
    return static_cast<T*>(prev->next);
  }

  unsigned get_client_data(Utcb &utcb, T *&data, unsigned identity) {
    T * client = 0;
    for (client = next(client); client && identity; client = next(client))
      if (client->get_identity() == identity) {
        data = reinterpret_cast<T *>(reinterpret_cast<unsigned long>(client));
        return ENONE;
      }

    //Logging::printf("gs: could not find client data for %x\n", identity);
    return EEXISTS;
  }

  /**
   * Returns a client which pseudonym does not exist anymore
   */
  T * get_invalid_client(Utcb &utcb, A * obj, T * client = 0) {
    unsigned err, crdout;
    for (client = next(client); client; client = next(client)) {
      err  = nova_syscall(NOVA_LOOKUP,  Crd(client->pseudonym, 0, DESC_CAP_ALL).value(), 0, 0, 0, &crdout);
      if (err || !crdout) return client;
    }
    return 0;
  }

  /**
   * Find all dead clients and mark them for removal by gc()
   */
  void cleanup_clients(Utcb &utcb, A * obj) {
    Guard guard_c(this, utcb, obj);
    T * client = get_invalid_client(utcb, obj);
    while (client) {
      //Logging::printf("gs: found dead client %p - freeing datastructure\n", client);
      free_client_data(utcb, client, obj);
      client = get_invalid_client(utcb, obj, client);
    }
  }
};


/**
 * Define a static portal function.
 */
template <class C> struct StaticPortalFunc {
  static void portal_func(cap_sel pid, C *tls, Utcb *utcb) __attribute__((regparm(1)))
  {
    bool free_cap;

    if (not utcb->validate_recv_bounds()) {
      utcb->msg[0]   = EPROTO;
      free_cap = (utcb->head.typed != 0);
      utcb->head.mtr = 1 /* untyped word */;
    } else {
      utcb->add_frame().head.untyped++; /* we will reply one word */
      Utcb::Frame input = utcb->get_nested_frame();
      free_cap = input.received_cap();
      utcb->msg[0] = tls->portal_func(*utcb, input, free_cap, pid);
      utcb->skip_frame();
      if (!free_cap && input.received_cap()) utcb->head.crd = tls->alloc_crd();
    }

    if (free_cap) {
      UNUSED unsigned res = nova_revoke(Crd(utcb->head.crd), true);
      assert(res == NOVA_ESUCCESS);
    }

    asmlinkage_protect("m"(tls), "m"(utcb));
  }
};
