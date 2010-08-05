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


/**
 * Timer service.
 *
 * State: unstable
 * Features: pit, hpet, worker-thread, xcpu-timeouts, shmem to communicate
 * Missing:  cache optimization
 */
class TimerService : public StaticReceiver<TimerService> {

  enum {
    CLIENTS = Config::MAX_CLIENTS+2,
    GRANULARITY = 1<<8
  };
  Motherboard &   _hostmb;
  Motherboard &   _mymb;
  TimeoutList<CLIENTS> _abs_timeouts;
  KernelSemaphore   _worker;


  struct ClientData {
    volatile unsigned reltime;
    ClientData * volatile lifo_next;
  } _data[CLIENTS];

  AtomicLifo<ClientData> _queue;

  /**
   * Do the actual work.
   */
  void work() __attribute__((noreturn))  {

    while (1) {
      _worker.down();
      COUNTER_INC("to work");


      unsigned nr;
      timevalue now = _mymb.clock()->time();
      ClientData *next;
      for (ClientData *head = _queue.dequeue_all(); head; head = next) {
	nr = head - _data;
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
      while ((nr = _abs_timeouts.trigger(now))) {
	MessageTimeout msg1(nr, now);
	_abs_timeouts.cancel(nr);
	_hostmb.bus_timeout.send(msg1);
      }

      // update timeout upstream
      if (_abs_timeouts.timeout() != ~0ULL) {
	MessageTimer msg2(0, _abs_timeouts.timeout());
	_mymb.bus_timer.send(msg2);
      }
    }
  }


public:


  bool  receive(MessageTimer &msg)
  {
    switch (msg.type) {
    case MessageTimer::TIMER_NEW:
      // XXX synchronization
      msg.nr = _abs_timeouts.alloc();
      break;
    case MessageTimer::TIMER_REQUEST_TIMEOUT:
      {
	assert(msg.nr < CLIENTS);
	long long diff = msg.abstime - _hostmb.clock()->time();
	if (diff < GRANULARITY)
	  diff = 1;
	else
	  if (diff <= (~0u)*GRANULARITY)
	    diff /= GRANULARITY;
	  else
	    diff = ~0u;

	_data[msg.nr].reltime = diff;
	asm volatile ("" : : : "memory");
	if (!_data[msg.nr].lifo_next) _queue.enqueue(_data+msg.nr);
	_worker.up();
      }
      break;
    default:
      return false;
    }
    return true;
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

  TimerService(Motherboard &hostmb) : _hostmb(hostmb), _mymb(*new Motherboard(hostmb.clock())), _data() {

    MessageHostOp msg0(MessageHostOp::OP_ALLOC_SEMAPHORE, 0);
    if (!hostmb.bus_hostop.send(msg0))
      Logging::panic("%s alloc semaphore failed", __func__);
    _worker = KernelSemaphore(msg0.value);

    // init timeouts
    _abs_timeouts.init();

    // prealloc timeouts for every module
    for (unsigned i=0; i < Config::MAX_CLIENTS; i++)  _abs_timeouts.alloc();


    // add to motherboards
    _hostmb.bus_timer.add(this, receive_static<MessageTimer>);
    _hostmb.bus_hostirq.add(this, receive_static<MessageIrq>);

    _mymb.bus_timeout.add(this, receive_static<MessageTimeout>);
    _mymb.bus_hostop.add(this, receive_static<MessageHostOp>);
    _mymb.bus_hwioout.add(this, receive_static<MessageIOOut>);
    _mymb.bus_acpi.add(this, receive_static<MessageAcpi>);

    // create backend devices
    char argv[] = "hostpit:1000,0x40,2 hosthpet";
    _mymb.parse_args(argv);

    // create the worker thread
    MessageHostOp msg2(MessageHostOp::OP_ALLOC_SERVICE_THREAD, reinterpret_cast<unsigned long>(this), reinterpret_cast<unsigned long>(TimerService::do_work));
    if (!hostmb.bus_hostop.send(msg2))
      Logging::panic("%s alloc service thread failed", __func__);
  }
};

PARAM(service_timer,
      new TimerService(mb);
      ,
      "service_timer - multiplexes either the hosthpet and hostpit between different clients");
