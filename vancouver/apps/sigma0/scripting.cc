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
  ScriptItem(Type _type, unsigned long _param0, unsigned long _param1) : type(_type), next(0), param0(_param0), param1(_param1) {}
};


/**
 * Simple scripting support.
 */
struct Script : public StaticReceiver<Script> {
  DBus<MessageTimer>  &_bus_timer;
  DBus<MessageConsole>&_bus_console;
  Clock               *_clock;
  unsigned             _timer;
  ScriptItem          *_head;


  void add(ScriptItem *item) {
    assert(!item->next);
    ScriptItem **start = &_head;
    while (*start) start = &(*start)->next;
    *start = item;
  }


  bool  receive(MessageLegacy &msg) {
    if (msg.type != MessageLegacy::RESET) return false;

    // prog a zero timeout
    MessageTimer msg1(_timer, _clock->abstime(0, 1000));
    check1(false, !_bus_timer.send(msg1));
    return true;
  }


  bool  receive(MessageTimeout &msg) {
    if (msg.nr != _timer)  return false;

    // XXX locking?
    while (_head) {
      Logging::printf(">> run script: ");
      ScriptItem * tmp = _head;
      ScriptItem item = *tmp;
      delete tmp;
      _head = item.next;

      switch (item.type) {
      case ScriptItem::TYPE_WAIT:
	{
	  MessageTimer msg1(_timer, _clock->abstime(item.param0, 1000));
	  Logging::printf("wait %ld abs %llx now %llx\n", item.param0,msg1.abstime, _clock->time());
	  check1(false, !_bus_timer.send(msg1));
	}
	return true;
      case ScriptItem::TYPE_START:
	Logging::printf("start %ld %ld\n", item.param0, item.param1);
	while (item.param1--) {
	  MessageConsole msg2(MessageConsole::TYPE_START, item.param0);
	  check1(false, !_bus_console.send(msg2));
	}
	break;
      default:
	assert(0);
      };
    }
    Logging::printf("<< script done;\n");
    return true;
  }
Script(DBus<MessageTimer> &bus_timer, DBus<MessageConsole> &bus_console, Clock *clock, unsigned timer)
  : _bus_timer(bus_timer), _bus_console(bus_console), _clock(clock), _timer(timer) {}
};


static Script *_script;
PARAM(script,
      // alloc a timer
      MessageTimer msg0;
      check0(!mb.bus_timer.send(msg0));
      _script = new Script(mb.bus_timer, mb.bus_console, mb.clock(), msg0.nr);
      mb.bus_timeout.add(_script, Script::receive_static<MessageTimeout>);
      mb.bus_legacy.add(_script,  Script::receive_static<MessageLegacy>);,
      "script - add scripting support")

PARAM(script_wait,
      check0(_script == 0);
      _script->add(new ScriptItem(ScriptItem::TYPE_WAIT, argv[0], 0));,
      "script_wait:t - wait t milliseconds until the next operation")

PARAM(script_start,
      check0(_script == 0);
      _script->add(new ScriptItem(ScriptItem::TYPE_START, argv[0], ~argv[1] ? argv[1] : 1));,
      "script_start:config,count=1 - start a config count times")
