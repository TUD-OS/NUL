/*
 * Timer Service.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011, Alexander Boettcherr <boettcher@tudos.org>
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

#include "nul/motherboard.h"
#include "nul/timer.h"
#include "sys/semaphore.h"
#include "service/lifo.h"

#include "nul/parent.h"
#include "nul/generic_service.h"
#include "nul/service_timer.h"
#include "nul/capalloc.h"

/**
 * Timer service.
 *
 * State: unstable
 * Features: pit, hpet, worker-thread, xcpu-timeouts, shmem to communicate
 * Missing:  hold _abs_timeouts-list in ClientData, remove nr, remove count and LAST_TIMEOUT,
 *           split gethosttime, make device list configurable
 */
class TimerService : public StaticReceiver<TimerService>, public CapAllocator<TimerService> {

  struct ClientData : public GenericClientData {
    volatile unsigned reltime;
    volatile unsigned count;
    unsigned nr;
    ClientData * volatile lifo_next;
  };

  enum {
    // Leave some room for services in sigma0 to allocate timers.
    CLIENTS = (1 << Config::MAX_CLIENTS_ORDER) + 10,
    GRANULARITY = 1<<8
  };
  Motherboard &   _hostmb;
  Motherboard &   _mymb;
  TimeoutList<CLIENTS, struct ClientData> _abs_timeouts;
  KernelSemaphore _worker;
  Semaphore       _clients;

  __attribute__((aligned(8))) ClientDataStorage<ClientData, TimerService> _storage;
  AtomicLifo<ClientData> _queue;
  char * flag_revoke;

  /**
   * Do the actual work.
   */
  void work() __attribute__((noreturn))  {

    while (1) {
      _worker.downmulti();
      COUNTER_INC("to work");

      unsigned nr;
      timevalue now = _mymb.clock()->time();
      ClientData *next;
      for (ClientData *head = _queue.dequeue_all(); head; head = next) {
        nr = head->nr;

        next = head->lifo_next;
        head->lifo_next = 0;

        MEMORY_BARRIER;

        // Dequeuing and reading reltime can race with setting reltime
        // and enqueuing in the REQUEST_TIMER code, when reprogramming
        // timers back-to-back. The requestor will enqueue the
        // ClientData instance again, but we have already programmed
        // the new timeout. When we dequeue it here, we program the
        // same timeout again. If the timeout triggered in between,
        // the client gets two wakeups, but since he called
        // REQUEST_TIMER twice, he has to cope with that.
        timevalue t = head->reltime;

        t *= GRANULARITY;
        _abs_timeouts.cancel(nr);
        _abs_timeouts.request(nr, now + t);
      }

      /**
       * Check whether timeouts have fired.
       */
      now = _mymb.clock()->time();
      {
        ClientDataStorage<ClientData, TimerService>::Guard guard_c(&_storage);
        ClientData volatile * data;
        while ((nr = _abs_timeouts.trigger(now, &data))) {
          _abs_timeouts.cancel(nr);
 
          if (data) { 
            data->count ++;
            unsigned res = nova_semup(data->identity);
            //should not happen, cap ->identity is allocated by service timer
            if (res != NOVA_ESUCCESS) Logging::panic("ts: sem cap disappeared res %x sm=0x%x\n", res, data->identity);
          }
        }
      }

      // update timeout upstream
      if (_abs_timeouts.timeout() != ~0ULL) {
        MessageTimer msg2(0, _abs_timeouts.timeout());
        _mymb.bus_timer.send(msg2);
        // Returns false in PIT-mode.
      }
    }
  }

  void check_clients(Utcb &utcb) {
    ClientDataStorage<ClientData, TimerService>::Guard guard_c(&_storage, utcb, this);
    ClientData volatile * data = _storage.get_invalid_client(utcb, this);
    while (data) {
      {
        //XXX more fine granular synchronization
        SemaphoreGuard l(_clients);
        _abs_timeouts.dealloc(data->nr);
      }
      Logging::printf("ts: found dead client - freeing datastructure\n");
      _storage.free_client_data(utcb, data, this);
      data = _storage.get_invalid_client(utcb, this, data);
    }
  }

public:

  inline unsigned alloc_crd() { return alloc_cap() << Utcb::MINSHIFT | DESC_TYPE_CAP; }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap) {
    unsigned res = ENONE;
    unsigned op;

    check1(EPROTO, input.get_word(op));

    switch (op) {
    case ParentProtocol::TYPE_OPEN:
      {
        ClientData *data = 0;
        res = _storage.alloc_client_data(utcb, data, input.received_cap(), this);
        if (res) {
          if (res != ERESOURCE) return res;
          check_clients(utcb); //force garbage collection run
          return ERETRY;
        }
        if (*flag_revoke) { check_clients(utcb); *flag_revoke = 0; }

        free_cap = false;

        {
          //XXX more fine granular synchronization
          SemaphoreGuard l(_clients);

          data->nr = _abs_timeouts.alloc(data);
        }
        if (!data->nr) return EABORT;

        utcb << Utcb::TypedMapCap(data->identity);
        //Logging::printf("ts:: new client data %x parent %x\n", data->identity, data->pseudonym);
        return res;
      }
    case ParentProtocol::TYPE_CLOSE:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, TimerService>::Guard guard_c(&_storage, utcb, this);
        check1(res, res = _storage.get_client_data(utcb, data, input.identity()));
        {
          //XXX more fine granular synchronization
          SemaphoreGuard l(_clients);
          _abs_timeouts.dealloc(data->nr);
        }
        Logging::printf("ts:: close session for %x\n", data->identity);
        return _storage.free_client_data(utcb, data, this);
      }
    case TimerProtocol::TYPE_REQUEST_TIMER:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, TimerService>::Guard guard_c(&_storage);
        if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

        TimerProtocol::MessageTimer msg = TimerProtocol::MessageTimer(0);
        if (input.get_word(msg)) return EABORT;

        COUNTER_INC("request to");

        assert(data->nr < CLIENTS);
        long long diff = msg.abstime - _hostmb.clock()->time();
        if (diff < GRANULARITY)
          diff = 1;
        else
          if (diff <= static_cast<long long>(~0u)*GRANULARITY)
            diff /= GRANULARITY;
          else
            diff = ~0u;

        data->reltime = diff;
        MEMORY_BARRIER;
        if (!data->lifo_next)
          _queue.enqueue(data);
        _worker.up();
      }
      return ENONE;
    case TimerProtocol::TYPE_REQUEST_LAST_TIMEOUT:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, TimerService>::Guard guard_c(&_storage);
        if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

        utcb << Cpu::xchg(&data->count, 0U);

        return ENONE;
      }
    case TimerProtocol::TYPE_REQUEST_TIME:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, TimerService>::Guard guard_c(&_storage);
        if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

        TimerProtocol::MessageTime _msg;
        if (!input.get_word(_msg)) return EABORT;
  
        MessageTime msg;
        msg.wallclocktime = _msg.wallclocktime;
        msg.timestamp = _msg.timestamp;
        if (_mymb.bus_time.send(msg)) {
          _msg.wallclocktime = msg.wallclocktime;
          _msg.timestamp = msg.timestamp;
          utcb << _msg;
          return ENONE;
        }

        return EABORT;
      }
    default:
      return EPROTO;
    }
  }

  bool  receive(MessageTimeout &msg) {
    _worker.up();
    return true;
  }

  // wrapper
  static void do_work(void *t) REGPARM(0) NORETURN { reinterpret_cast<TimerService *>(t)->work(); }
  bool  receive(MessageHostOp  &msg) { return _hostmb.bus_hostop.send(msg); }
  bool  receive(MessageIOOut &msg)   { return _hostmb.bus_hwioout.send(msg); }
  bool  receive(MessageIOIn &msg)    { return _hostmb.bus_hwioin.send(msg); }
  bool  receive(MessageAcpi &msg)    { return _hostmb.bus_acpi.send(msg); }
  bool  receive(MessageIrq &msg)     { return _mymb.bus_hostirq.send(msg, true); }

  TimerService(Motherboard &hostmb, unsigned _cap, unsigned _cap_order, char * revoke_mem)
    : CapAllocator<TimerService> (_cap, _cap, _cap_order), _hostmb(hostmb), _mymb(*new Motherboard(hostmb.clock(), hostmb.hip())), flag_revoke(revoke_mem) {

    MessageHostOp msg0(MessageHostOp::OP_ALLOC_SEMAPHORE, 0UL);
    if (!hostmb.bus_hostop.send(msg0))
      Logging::panic("%s alloc semaphore failed", __func__);
    _worker = KernelSemaphore(msg0.value);

    MessageHostOp msg1(MessageHostOp::OP_ALLOC_SEMAPHORE, 0UL);
    if (!hostmb.bus_hostop.send(msg1))
      Logging::panic("%s alloc semaphore failed", __func__);
    _clients = Semaphore(msg1.value);
    _clients.up();

    // init timeouts
    _abs_timeouts.init();

    // add to motherboards
    _hostmb.bus_hostirq.add(this, receive_static<MessageIrq>);

    _mymb.bus_timeout.add(this, receive_static<MessageTimeout>);
    _mymb.bus_hostop.add(this, receive_static<MessageHostOp>);
    _mymb.bus_hwioout.add(this, receive_static<MessageIOOut>);
    _mymb.bus_hwioin.add(this, receive_static<MessageIOIn>);
    _mymb.bus_acpi.add(this, receive_static<MessageAcpi>);

    // create backend devices
    _mymb.parse_args("hostpit:1000,0x40,2 hosthpet hostrtc");

    // create the worker thread
    MessageHostOp msg2 = MessageHostOp::alloc_service_thread(TimerService::do_work,
                                                             this, 1UL);
    if (!hostmb.bus_hostop.send(msg2))
      Logging::panic("%s alloc service thread failed", __func__);
  }

  void * operator new (unsigned size, unsigned alignment) { return  new (alignment) char [sizeof(TimerService)]; }
};

PARAM(service_timer,
      unsigned cap_region = alloc_cap_region(1 << 12, 12);
      char * revoke_mem = new (0x1000) char[0x1000];
      TimerService * t = new (8) TimerService(mb, cap_region, 12, revoke_mem);

      //Logging::printf("cap region timer %x %x\n", cap_region, mb.hip()->cfg_cap);
      MessageHostOp msg(t, "/timer", reinterpret_cast<unsigned long>(StaticPortalFunc<TimerService>::portal_func), revoke_mem);
      msg.crd_t = Crd(cap_region, 12, DESC_TYPE_CAP).value();
      if (!cap_region || !mb.bus_hostop.send(msg))
        Logging::panic("starting of timer service failed");
      ,
      "service_timer - multiplexes either the hosthpet or the hostpit between different clients");
