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


/**
 * Timer service.
 *
 * State: unstable
 * Features: pit, hpet, worker-thread, xcpu-timeouts, shmem for
 */
class TimerService : public StaticReceiver<TimerService> {

  Motherboard &   _hostmb;
  Motherboard &   _mymb;
  Semaphore       _lock;
  TimeoutList<Config::MAX_CLIENTS+2> _abs_timeouts;
  Semaphore       _worker;
  Semaphore       _relsem;
  volatile unsigned  _relnr;
  volatile timevalue _reltime;

  /**
   * Do the actual work.
   */
  void work() __attribute__((noreturn))  {

    while (1) {
      _worker.down();
      COUNTER_INC("to work");

      /**
       * Check whether a new timeout should be programmed.
       */
      timevalue now = _mymb.clock()->time();
      unsigned nr = _relnr;
      if (nr != ~0u) {

	_abs_timeouts.cancel(nr);
	_abs_timeouts.request(nr, now + _reltime);
	_relnr = ~0u;
	_relsem.up();
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
	/**
	 * We assume here that the alloc function can be done in
	 * parallel to the worker, as we lock different callers
	 * against each other, but the worker does not lock access to
	 * _abs_timeouts.
	 */
	SemaphoreGuard l(_relsem);
	msg.nr = _abs_timeouts.alloc();
      }
      break;
    case MessageTimer::TIMER_REQUEST_TIMEOUT:
      {
	// request the entry
	_relsem.down();

	// it has to be empty
	assert(_relnr == ~0u);

	timevalue now = _hostmb.clock()->time();
	if (now > msg.abstime)  now = msg.abstime;
	_reltime = msg.abstime - now;
	_relnr   = msg.nr;
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

  TimerService(Motherboard &hostmb) : _hostmb(hostmb), _mymb(*new Motherboard(hostmb.clock())),  _relnr(~0u) {

    MessageHostOp msg0(MessageHostOp::OP_ALLOC_SEMAPHORE, 0);
    MessageHostOp msg1(MessageHostOp::OP_ALLOC_SEMAPHORE, 0);
    if (!hostmb.bus_hostop.send(msg0) || !hostmb.bus_hostop.send(msg1))
      Logging::panic("%s alloc semaphore failed", __func__);
    _worker = Semaphore(msg0.value);
    _relsem = Semaphore(msg1.value);

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

    // make sure everybody can use it
    _relsem.up();

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
