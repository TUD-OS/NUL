/*
 * Timer Service.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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


/**
 * Timer service.
 *
 * State: unstable
 * Features: pit, hpet, worker-thread, xcpu-timeouts
 * Missing:  lock-free-xcpu-communication?
 */
class TimerService : public StaticReceiver<TimerService> {

  TimeoutList<Config::MAX_CLIENTS+2> _rel_timeouts;
  TimeoutList<Config::MAX_CLIENTS+2> _abs_timeouts;
  Motherboard &   _hostmb;
  Motherboard &   _mymb;
  long            _lockcount;
  Semaphore       _lock;
  KernelSemaphore _worker;

  /**
   * Do the actual work.
   */
  void work() __attribute__((noreturn))  {

    while (1) {
      _worker.down();

      /**
       * Check whether new timeouts should be programmed.
       */
      unsigned nr;
      timevalue now = _mymb.clock()->time();
      {
	SemaphoreGuard l(_lock);
	timevalue rel;
	while ((rel = _rel_timeouts.timeout()) != ~0ull) {
	  nr = _rel_timeouts.trigger(rel);
	  _rel_timeouts.cancel(nr);
	  _abs_timeouts.request(nr, now+rel);
	}
      }


      /**
       * Check whether timeouts have fired.
       */
      while ((nr = _abs_timeouts.trigger(now))) {
	MessageTimeout msg1(nr, now);
	_abs_timeouts.cancel(nr);
	_hostmb.bus_timeout.send(msg1);
      }

      // update timeout upstream
      if (_abs_timeouts.timeout() != ~0ULL) {
	MessageTimer msg2(0, _abs_timeouts.timeout());
	//Logging::printf("prog to %llx\n", _abs_timeouts.timeout());
	_mymb.bus_timer.send(msg2);
      }
    }
  }


public:


  bool  receive(MessageTimer &msg)
  {
    switch (msg.type) {
    case MessageTimer::TIMER_NEW:
      {
	SemaphoreGuard l(_lock);
	msg.nr = _rel_timeouts.alloc();
	unsigned n = _abs_timeouts.alloc();
	assert(n == msg.nr);
      }
      break;
    case MessageTimer::TIMER_REQUEST_TIMEOUT:
      {
	timevalue now = _hostmb.clock()->time();
	if (now > msg.abstime)  now = msg.abstime;

	SemaphoreGuard l(_lock);
	_rel_timeouts.request(msg.nr, msg.abstime - now);
      }
      _worker.up();
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
  bool  receive(MessageIrq &msg)     { return _mymb.bus_hostirq.send(msg); }

  TimerService(Motherboard &hostmb) : _hostmb(hostmb), _mymb(*new Motherboard(hostmb.clock())) {

    MessageHostOp msg0(MessageHostOp::OP_ALLOC_SEMAPHORE, 0);
    MessageHostOp msg1(MessageHostOp::OP_ALLOC_SEMAPHORE, 0);
    if (!hostmb.bus_hostop.send(msg0) || !hostmb.bus_hostop.send(msg1))
      Logging::panic("alloc semaphore failed");
    _lock = Semaphore(&_lockcount, msg0.value);
    _worker = KernelSemaphore(msg1.value);
    Logging::printf("2 caps %lx %lx\n", msg0.value, msg1.value);

    // init timeouts
    _rel_timeouts.init();
    _abs_timeouts.init();

    // prealloc timeouts for every module
    for (unsigned i=0; i < Config::MAX_CLIENTS; i++) {
      _rel_timeouts.alloc();
      _abs_timeouts.alloc();
    }
    _hostmb.bus_timer.add(this, receive_static<MessageTimer>);
    _hostmb.bus_hostirq.add(this, receive_static<MessageIrq>);

    _mymb.bus_timeout.add(this, receive_static<MessageTimeout>);
    _mymb.bus_hostop.add(this, receive_static<MessageHostOp>);
    _mymb.bus_hwioout.add(this, receive_static<MessageIOOut>);
    _mymb.bus_acpi.add(this, receive_static<MessageAcpi>);
    char argv[] = "hostpit:1000,0x40,2 hosthpet";
    _mymb.parse_args(argv);

    // make sure everybody can use it
    _lock.up();

    // create the worker thread
    MessageHostOp msg2(MessageHostOp::OP_ALLOC_SERVICE_THREAD, reinterpret_cast<unsigned long>(this), reinterpret_cast<unsigned long>(TimerService::do_work));
    if (!hostmb.bus_hostop.send(msg2))
      Logging::panic("alloc service thread failed");
  }
};


PARAM(service_timer,
      new TimerService(mb);
      ,
      "service_timer - multiplexes either the hosthpet and hostpit between different clients");
