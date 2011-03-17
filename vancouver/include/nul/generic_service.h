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
 * Data that is stored by every client.
 */
struct GenericClientData {
  void volatile * volatile next;
  void volatile * volatile del;
  unsigned           pseudonym;
  unsigned           identity;
  /**
   * We implement a get_quota here, so that derived classes can
   * overwrite it.  Usefull for example in sigma0 that has a different
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


/**
 * A generic container that stores per-client data.
 * Missing: dequeue-from-list, iterator
 */
template <class T, class A, bool free_pseudonym = true, bool __DEBUG__ = false>
class ClientDataStorage {
  struct recycl {
    T volatile * volatile head;
    unsigned long volatile t_in;
  };
  struct recycl_nv {
    T * head;
    unsigned long volatile t_in;
  };

  T volatile * volatile _head;
  union {
    ALIGNED(8) struct recycl recycling;
    ALIGNED(8) unsigned long long volatile recyc64;
  };


public:
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
        if (obj) storage->cleanup(*utcb, obj);
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
        if (obj) storage->cleanup(*utcb, obj);
      }
  };

  ClientDataStorage() : _head(0), recyc64(0) {
    assert(sizeof(T *) == 4);
    assert(!(reinterpret_cast<unsigned long>(&recyc64) & 0x7));
  }

  unsigned alloc_client_data(Utcb &utcb, T *&data, unsigned pseudonym, A * obj) {
    unsigned res;
    QuotaGuard<T> guard1(utcb, pseudonym, "mem", sizeof(T));
    QuotaGuard<T> guard2(utcb, pseudonym, "cap", 2, &guard1);
    check1(res, res = guard2.status());
    guard2.commit();

    unsigned identity = obj->alloc_cap();
    if (!identity) return ERESOURCE;
    data = new T;
    if (!data) return ERESOURCE;

    memset(data, 0, sizeof(T));
    data->pseudonym = pseudonym;
    data->identity  = identity;
    res = nova_create_sm(data->identity);
    if (res != ENONE) {
      obj->dealloc_cap(identity);
      delete data;
      data = 0;
      return res;
    }

    // enqueue it
    T volatile * __head;
    do {
      __head = _head;
      data->next = __head;
    } while (reinterpret_cast<unsigned long>(__head) != Cpu::cmpxchg4b(&_head, reinterpret_cast<unsigned long>(__head), reinterpret_cast<unsigned long>(data)));
    if (__DEBUG__) Logging::printf("gs: alloc_client_data %p\n", data);
    return ENONE;
  }

  unsigned free_client_data(Utcb &utcb, T volatile *data, A * obj) {
    Guard count(this, utcb, obj);

    for (T volatile * volatile * prev = &_head; T volatile * volatile current = *prev; prev = reinterpret_cast<T volatile * volatile *>(&current->next))
      if (current == data) {

        again:

        void volatile * volatile data_next = data->next;
        if (reinterpret_cast<unsigned long>(data) != Cpu::cmpxchg4b(prev, reinterpret_cast<unsigned long>(data), reinterpret_cast<unsigned long>(data_next))) {
          if (__DEBUG__) Logging::printf("gs: another thread concurrently removed same data item - nothing todo\n");
          return 0;
        }

        if (data->next != data_next) {
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

          void volatile * tmp2_next = data->next;
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
            if (data->next != tmp2_next) {
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
        for (prev = &_head; current = *prev; prev = reinterpret_cast<T volatile * volatile *>(&current->next))
          if (current == data) {
            if (__DEBUG__) Logging::printf("gs: still in list - again loop %p\n", data);
            goto again;
          }
        //BBB end

        T volatile * tmp;
        do {
          tmp = recycling.head;
          data->del = tmp;
        } while (reinterpret_cast<unsigned long>(tmp) != Cpu::cmpxchg4b(&recycling.head, reinterpret_cast<unsigned long>(tmp), reinterpret_cast<unsigned long>(data)));

        return 0;
      }

    if (__DEBUG__) {
      Logging::printf("didn't found item %p\n list:", data);
    
      for (T volatile * volatile * prev = &_head; T volatile * current = *prev; prev = reinterpret_cast<T volatile * volatile *>(&current->next))
        Logging::printf(" %p ->", current);
    }
    assert(0);
  }

  /**
   * Iterator.
   */
  T volatile * next(T volatile *prev = 0) {
    assert(recycling.t_in);
    if (!prev) return reinterpret_cast<T volatile *>(_head);
    return reinterpret_cast<T volatile *>(prev->next);
  }

  /**
   * Remove items which are unused 
   */
  void cleanup(Utcb &utcb, A * obj) {
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
          if (crdout) {
            T::get_quota(utcb, data->pseudonym,"cap", -2);
            T::get_quota(utcb, data->pseudonym, "mem", -sizeof(T));
            data->session_close(utcb);
          }
          obj->dealloc_cap(data->identity);
          if (free_pseudonym) obj->dealloc_cap(data->pseudonym);
          //tmp = data->del;
          unsigned long nv_del = reinterpret_cast<unsigned long>(&data->del);
          tmp = *reinterpret_cast<T **>(nv_del);
          delete data;
          if (__DEBUG__) Logging::printf("gs: delete %p\n", data);
          data = tmp;
          counter ++;
        }
        if (__DEBUG__) Logging::printf("gs: cleaned objects 0x%lx\n", counter);
      } else
        if (__DEBUG__) Logging::printf("gs: did not get cleaning list\n");
    }
  }

  unsigned get_client_data(Utcb &utcb, T *&data, unsigned identity) {
    T volatile * client = 0;
    for (client = next(client); client && identity; client = next(client))
      if (client->identity == identity) {
        data = reinterpret_cast<T *>(reinterpret_cast<unsigned long>(client));
        return ENONE;
      }

    //Logging::printf("gs: could not find client data for %x\n", identity);
    return EEXISTS;
  }

  /**
   * Returns a client which pseudonym does not exist anymore
   */
  T volatile * get_invalid_client(Utcb &utcb, A * obj, T volatile * client = 0) {
    unsigned err, crdout;
    for (client = next(client); client; client = next(client)) {
      err  = nova_syscall(NOVA_LOOKUP,  Crd(client->pseudonym, 0, DESC_CAP_ALL).value(), 0, 0, 0, &crdout);
      if (err || !crdout) return client;
    }
    return 0;
  }
};


/**
 * Define a static portal function.
 */
template <class C> struct StaticPortalFunc {
  static void portal_func(C *tls, Utcb *utcb) __attribute__((regparm(0)))
  {
    utcb->add_frame().head.untyped++;
    Utcb::Frame input = utcb->get_nested_frame();
    bool free_cap = input.received_cap();
    utcb->msg[0] = tls->portal_func(*utcb, input, free_cap);
    utcb->skip_frame();
    if (free_cap) {
      unsigned res = nova_revoke(Crd(utcb->head.crd), true);
      assert(res == NOVA_ESUCCESS);
    }
    else if (input.received_cap())
      utcb->head.crd = tls->alloc_crd();
    asmlinkage_protect("g"(tls), "g"(utcb));
  }
};
