/*
 * Simple scripting support.
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
#include "nul/parent.h"

#include "nul/baseprogram.h"
#include "nul/service_timer.h"

/**
 * Single script item.
 */
struct ScriptItem {
  enum Type {
    TYPE_WAIT,
    TYPE_START,
  } type;
  ScriptItem *next;
  unsigned long param0;
  unsigned long param1;
  unsigned long param2;
  ScriptItem(Type _type, unsigned long _param0, unsigned long _param1, unsigned long _param2)
    : type(_type), next(0), param0(_param0), param1(_param1), param2(_param2) {}
};


/**
 * Simple scripting support.
 */
struct Script : public StaticReceiver<Script> {
  TimerProtocol       *_service_timer;
  DBus<MessageConsole>&_bus_console;
  Clock               *_clock;
  KernelSemaphore      _worker;
  ScriptItem          *_head;
  /**
   * Do the actual work.
   */
  void work(Utcb &utcb) __attribute__((noreturn))  {
    while (1) {
      _worker.down();

      while (_head) {
        Logging::printf(">> run script: ");
        ScriptItem * tmp = _head;
        ScriptItem item = *tmp;
        delete tmp;
        _head = item.next;

        if (item.type == ScriptItem::TYPE_WAIT) {
          TimerProtocol::MessageTimer msg1(_clock->abstime(item.param0, 1000));
          Logging::printf("wait %ld abs %llx now %llx\n", item.param0,msg1.abstime, _clock->time());
          if (_service_timer->timer(utcb, msg1))
            Logging::printf("setting timeout failed\n");
          break;
      	} else
        if (item.type ==ScriptItem::TYPE_START) {
          Logging::printf("start %ld-%ld count %ld\n", item.param0, item.param1, item.param2);
          bool cont = false;
          while (item.param2--) {
            for (unsigned long nr = 0; nr < item.param1; nr++) {
              MessageConsole msg2(MessageConsole::TYPE_START, item.param0 + nr);
              if (!_bus_console.send(msg2)) {
                cont = (nr != item.param0);
                break;
              }
            }
          }
          if (cont) continue;
          break;
        } else
          assert(0);
      }
      Logging::printf("<< script done;\n");
    }
  }

  // wrapper
  static void do_work(void *u, void *t, Utcb * utcb)  __attribute__((regparm(1), noreturn)) { reinterpret_cast<Script *>(t)->work(*utcb); }

  void add(ScriptItem *item) {
    assert(!item->next);
    ScriptItem **start = &_head;
    while (*start) start = &(*start)->next;
    *start = item;
  }


  bool  receive(MessageLegacy &msg) {
    if (msg.type != MessageLegacy::RESET) return false;

    // prog a zero timeout
    TimerProtocol::MessageTimer msg1(_clock->abstime(0, 1000));
    check1(false, _service_timer->timer(*BaseProgram::myutcb(),msg1));
    return true;
  }

  Script(DBus<MessageConsole> &bus_console, Clock *clock, Motherboard * mb)
    : _bus_console(bus_console), _clock(clock)
  {

    // alloc a timer
    _service_timer = new TimerProtocol(alloc_cap(TimerProtocol::CAP_NUM));

    unsigned res;
    TimerProtocol::MessageTimer msg1(_clock->abstime(0, 1000));
    if ((res = _service_timer->timer(*BaseProgram::myutcb(), msg1)))
      Logging::panic("setting timeout failed with %x\n", res);

    _worker = KernelSemaphore(_service_timer->get_notify_sm());

    // create the worker thread
    MessageHostOp msg(MessageHostOp::OP_ALLOC_SERVICE_THREAD, reinterpret_cast<unsigned long>(this), 1);
    msg.ptr = reinterpret_cast<char *>(Script::do_work);
    if (!mb->bus_hostop.send(msg))
      Logging::panic("%s alloc service thread failed", __func__);

  }
};

static Script *_script;
PARAM(script,
      _script = new Script(mb.bus_console, mb.clock(), &mb);

      mb.bus_legacy.add(_script,  Script::receive_static<MessageLegacy>);
      ,
      "script - add scripting support")

PARAM(script_wait,
      check0(_script == 0);
      _script->add(new ScriptItem(ScriptItem::TYPE_WAIT, argv[0], 0, 0));,
      "script_wait:t - wait t milliseconds until the next scripting operation will happen")

PARAM(script_start,
      check0(_script == 0);
      _script->add(new ScriptItem(ScriptItem::TYPE_START, ~argv[0] ? argv[0] - 1 : 0, ~argv[1] ? argv[1] : 1, ~argv[2] ? argv[1] : 1));,
      "script_start:config=1,number=1,count=1 - start a config count times",
      "Example: 'script_start:5,3,4' - starts 4 times the configs 5, 6 and 7")
