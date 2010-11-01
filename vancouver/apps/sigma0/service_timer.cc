/*
 * Timer Service.
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

#include "nul/motherboard.h"
#include "nul/timer.h"
#include "sys/semaphore.h"
#include "service/lifo.h"

#include "nul/parent.h"
#include "nul/generic_service.h"
#include "nul/service_timer.h"

/**
 * Timer service.
 *
 * State: unstable
 * Features: pit, hpet, worker-thread, xcpu-timeouts, shmem to communicate
 * Missing:  cache optimization
 */
class TimerService : public StaticReceiver<TimerService> {

  struct ClientData : public GenericClientData {
    volatile unsigned reltime;
    volatile unsigned count;
    unsigned nr;
    ClientData * volatile lifo_next;
  };

  enum {
    // Leave some room for services in sigma0 to allocate timers.
    CLIENTS = Config::MAX_CLIENTS + 10,
    GRANULARITY = 1<<8
  };
  Motherboard &   _hostmb;
  Motherboard &   _mymb;
  TimeoutList<CLIENTS, struct ClientData> _abs_timeouts;
  KernelSemaphore   _worker;


  ClientDataStorage<ClientData> _storage;
  AtomicLifo<ClientData> _queue;

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
        asm volatile("");
        timevalue t = Cpu::xchg(&head->reltime, 0);
        t *= GRANULARITY;
        _abs_timeouts.cancel(nr);
        _abs_timeouts.request(nr, now + t);
      }

      /**
       * Check whether timeouts have fired.
       */
      now = _mymb.clock()->time();
      ClientData * data;
      while ((nr = _abs_timeouts.trigger(now, &data))) {
        _abs_timeouts.cancel(nr);

        assert(data);
        data->count ++;
        nova_semup(data->identity);
      }

      // update timeout upstream
      if (_abs_timeouts.timeout() != ~0ULL) {
        MessageTimer msg2(0, _abs_timeouts.timeout());
        assert(_mymb.bus_timer.send(msg2));
      }
    }
  }


public:

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap) {
    ClientData *data = 0;
    unsigned res = ENONE;
    unsigned op;

    check1(EPROTO, input.get_word(op));

    switch (op) {
    case ParentProtocol::TYPE_OPEN:
      check1(res, res = _storage.alloc_client_data(utcb, data, input.received_cap()));
      free_cap = false;

      //XXX synchronization
      data->nr = _abs_timeouts.alloc(data);
      if (!data->nr) return EABORT;

      utcb << Utcb::TypedMapCap(data->identity);
      Logging::printf("ts:: new client data %x parent %x\n", data->identity, data->pseudonym);
      return res;
    case ParentProtocol::TYPE_CLOSE:
      check1(res, res = _storage.get_client_data(utcb, data, input.identity()));
      Logging::printf("ts:: close session for %x\n", data->identity);
      return _storage.free_client_data(utcb, data);
    case TimerProtocol::TYPE_REQUEST_TIMER:
      {
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
        asm volatile ("" : : : "memory");
        if (!data->lifo_next)
          _queue.enqueue(data);
        _worker.up();
      }
      return ENONE;
    case TimerProtocol::TYPE_REQUEST_LAST_TIMEOUT:
      {
        if ((res = _storage.get_client_data(utcb, data, input.identity())))  return res;

        utcb << data->count;

        data->count = 0;

        return ENONE;
      }
    case TimerProtocol::TYPE_REQUEST_TIME:
      {
        if ((res = _storage.get_client_data(utcb, data, input.identity())))  return res;

        TimerProtocol::MessageTime _msg;
        if (!input.get_word(_msg)) return EABORT;
  
        MessageTime msg;
        msg.wallclocktime = _msg.wallclocktime;
        msg.timestamp = _msg.timestamp;
        if (_hostmb.bus_time.send(msg)) {
  	      // XXX we assume the same mb->clock() source here
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
  static void do_work(void *u, void *t)  __attribute__((regparm(1), noreturn)) { reinterpret_cast<TimerService *>(t)->work(); }
  bool  receive(MessageHostOp  &msg) { return _hostmb.bus_hostop.send(msg); }
  bool  receive(MessageIOOut &msg)   { return _hostmb.bus_hwioout.send(msg); }
  bool  receive(MessageAcpi &msg)    { return _hostmb.bus_acpi.send(msg); }
  bool  receive(MessageIrq &msg)     { return _mymb.bus_hostirq.send(msg, true); }

  TimerService(Motherboard &hostmb) : _hostmb(hostmb), _mymb(*new Motherboard(hostmb.clock())) {

    MessageHostOp msg0(MessageHostOp::OP_ALLOC_SEMAPHORE, 0);
    if (!hostmb.bus_hostop.send(msg0))
      Logging::panic("%s alloc semaphore failed", __func__);
    _worker = KernelSemaphore(msg0.value);

    // init timeouts
    _abs_timeouts.init();

    // add to motherboards
    _hostmb.bus_hostirq.add(this, receive_static<MessageIrq>);

    _mymb.bus_timeout.add(this, receive_static<MessageTimeout>);
    _mymb.bus_hostop.add(this, receive_static<MessageHostOp>);
    _mymb.bus_hwioout.add(this, receive_static<MessageIOOut>);
    _mymb.bus_acpi.add(this, receive_static<MessageAcpi>);

    // create backend devices
    _mymb.parse_args("hostpit:1000,0x40,2 hosthpet");

    // create the worker thread
    MessageHostOp msg2(MessageHostOp::OP_ALLOC_SERVICE_THREAD, reinterpret_cast<unsigned long>(this), 1);
    msg2.ptr = reinterpret_cast<char *>(TimerService::do_work);
    if (!hostmb.bus_hostop.send(msg2))
      Logging::panic("%s alloc service thread failed", __func__);
  }
};

PARAM(service_timer,
      TimerService * t = new TimerService(mb);

      MessageHostOp msg(MessageHostOp::OP_REGISTER_SERVICE, reinterpret_cast<unsigned long>(StaticPortalFunc<TimerService>::portal_func), reinterpret_cast<unsigned long>(t));
      msg.ptr = const_cast<char *>("/timer");
      if (!mb.bus_hostop.send(msg))
        Logging::panic("registering timer service failed");
      ,
      "service_timer - multiplexes either the hosthpet and hostpit between different clients");
