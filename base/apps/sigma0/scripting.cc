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
    TYPE_REBOOT,
    TYPE_WAITCHILD,
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
  DBus<MessageHostOp> &_bus_hostop;
  Clock               *_clock;
  KernelSemaphore      _worker;
  ScriptItem          *_head;
  /**
   * Do the actual work.
   */
  void work() __attribute__((noreturn))  {
    Utcb &utcb = *BaseProgram::myutcb();
    while (1) {
      _worker.down();

      bool work_done = false;
      unsigned last_child_id = ~0U;
      while (_head) {
        work_done = true;

        ScriptItem * tmp = _head;
        ScriptItem item = *tmp;
        delete tmp;
        _head = item.next;

        if (item.type == ScriptItem::TYPE_WAIT) {
          Logging::printf("sc: wait %ldms\n", item.param0);
          if (_service_timer->timer(utcb, _clock->abstime(item.param0, 1000)))
            Logging::printf("sc: setting timeout failed\n");
          break;
      	} else
        if (item.type ==ScriptItem::TYPE_START) {
          Logging::printf("sc: start %ld-%ld count %ld\n", item.param0, item.param1, item.param2);
          bool cont = true;
          while (item.param2--) {
            for (unsigned long nr = 0; nr < item.param1; nr++) {
              MessageConsole msg2(MessageConsole::TYPE_START, item.param0 + nr);
              if (!_bus_console.send(msg2)) {
                cont = (nr != item.param0);
                break;
              }
	      last_child_id = msg2.id;
            }
          }
          if (cont) continue;
          break;
        } else
          if (item.type == ScriptItem::TYPE_REBOOT) {
            Logging::printf("sc: reboot\n");
            MessageConsole m(MessageConsole::TYPE_RESET);
            _bus_console.send(m);
          } else if (item.type == ScriptItem::TYPE_WAITCHILD) {
	    if (last_child_id != ~0U) {
	      Logging::printf("sc: wait for child %d\n", last_child_id);
	      MessageHostOp m(MessageHostOp::OP_WAIT_CHILD, last_child_id);
	      _bus_hostop.send(m);
	    } else
	      Logging::printf("sc: error: Nobody to wait for! %d\n", last_child_id);
	  } else
          assert(0);
      }

      if (work_done)
        Logging::printf("sc: %s.\n", _head ? "waiting.." : "done");
    }
  }

  // wrapper
  static void do_work(void *t)  REGPARM(0) NORETURN { reinterpret_cast<Script *>(t)->work(); }

  void add(ScriptItem *item) {
    assert(!item->next);
    ScriptItem **start = &_head;
    while (*start) start = &(*start)->next;
    *start = item;
  }


  bool  receive(MessageLegacy &msg) {
    if (msg.type != MessageLegacy::RESET) return false;

    // prog a zero timeout
    check1(false, _service_timer->timer(*BaseProgram::myutcb(), _clock->abstime(0, 1000)));
    return true;
  }

  Script(DBus<MessageConsole> &bus_console, Clock *clock, Motherboard * mb)
    : _bus_console(bus_console), _bus_hostop(mb->bus_hostop), _clock(clock)
  {

    // alloc a timer
    _service_timer = new TimerProtocol(alloc_cap_region(TimerProtocol::CAP_SERVER_PT + mb->hip()->cpu_desc_count(), 0));

    unsigned res;
    if ((res = _service_timer->timer(*BaseProgram::myutcb(), _clock->abstime(0, 1000))))
      Logging::panic("sc: setting timeout failed with %x\n", res);

    _worker = KernelSemaphore(_service_timer->get_notify_sm());

    // create the worker thread
    MessageHostOp msg = MessageHostOp::alloc_service_thread(Script::do_work,
                                                            this, "scripting", 1UL);
    if (!_bus_hostop.send(msg))
      Logging::panic("sc: %s alloc service thread failed", __func__);
  }
};

static Script *_script;
PARAM_HANDLER(script,
	      "script - add scripting support")
{
  _script = new Script(mb.bus_console, mb.clock(), &mb);
  mb.bus_legacy.add(_script,  Script::receive_static<MessageLegacy>);
}

PARAM_HANDLER(script_wait,
	      "script_wait:t - wait t milliseconds until the next scripting operation will happen")
{
  check0(_script == 0);
  _script->add(new ScriptItem(ScriptItem::TYPE_WAIT, argv[0], 0, 0));
}

PARAM_HANDLER(script_start,
	      "script_start:config=1,number=1,count=1 - start a config count times",
	      "Example: 'script_start:5,3,4' - starts 4 times the configs 5, 6 and 7")
{
  check0(_script == 0);
  _script->add(new ScriptItem(ScriptItem::TYPE_START, ~argv[0] ? argv[0] - 1 : 0, ~argv[1] ? argv[1] : 1, ~argv[2] ? argv[2] : 1));
}

PARAM_HANDLER(script_waitchild,
	      "script_waitchild - wait for event from the last started child")
{
  check0(_script == 0);
  _script->add(new ScriptItem(ScriptItem::TYPE_WAITCHILD, 0, 0, 0));
}

PARAM_HANDLER(script_reboot,
	      "script_reboot - schedule a reboot")
{
  check0(_script == 0);
  _script->add(new ScriptItem(ScriptItem::TYPE_REBOOT, 0, 0 ,0));
}

// EOF
