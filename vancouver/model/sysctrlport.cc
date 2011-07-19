/** @file
 * PS2 system control port emulation.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "nul/motherboard.h"


/**
 * Implements the systemcontrol ports A+B of a PS/2 system.
 *
 * State: testing
 * Features: FastA20, INIT, PIT_C1_OUT, PIT_C2_OUT, PIT_C2_GATE
 * Missing: A:ActivityLight, A:CMOSLock, A:WatchdogTimeout, B:NMI, B:Parity, B:SpeakerData
 */
class SystemControlPort : public StaticReceiver<SystemControlPort>
{
  DBus<MessageLegacy> &_bus_legacy;
  DBus<MessagePit>    &_bus_pit;
  unsigned _port_a;
  unsigned _port_b;
  unsigned char _last_porta;
  unsigned char _last_portb;

 public:

  bool  receive(MessageIOIn &msg)
  {
    if (msg.type != MessageIOIn::TYPE_INB) return false;
    if (msg.port == _port_a)
      {
	msg.value = _last_porta & 0x3;
	return true;
      }

    if (msg.port == _port_b)
      {
	msg.value = 0;
	MessagePit msg2(MessagePit::GET_OUT, 1);
	if (_bus_pit.send(msg2))
	  msg.value |= msg2.value << 4;
	msg2.pit = 2;
	if (_bus_pit.send(msg2))
	  msg.value |= msg2.value << 5;
	msg.value |= _last_portb & 1;
	return true;
      }
    return false;
  }


  bool  receive(MessageIOOut &msg)
  {
    if (msg.type != MessageIOOut::TYPE_OUTB) return false;
    if (msg.port == _port_a)
      {
	// fast A20 gate
	if ((_last_porta ^ msg.value) & 2)
	  {
	    MessageLegacy msg2(MessageLegacy::FAST_A20, ((msg.value & 2) >> 1));
	    _bus_legacy.send(msg2);
	  }
	if (!(_last_porta & 1) && msg.value & 1)
	  {
	    MessageLegacy msg2(MessageLegacy::INIT, 1);
	    _bus_legacy.send(msg2);
	  }
	_last_porta = msg.value;
	return true;
      }

    if (msg.port == _port_b)
      {
	_last_portb = msg.value;
	MessagePit msg2(MessagePit::SET_GATE, 2, msg.value & 1);
	_bus_pit.send(msg2);
	return true;
      }
    return false;
  }


  SystemControlPort(DBus<MessageLegacy> &bus_legacy, DBus<MessagePit> &bus_pit, unsigned port_a, unsigned port_b)
    : _bus_legacy(bus_legacy), _bus_pit(bus_pit), _port_a(port_a), _port_b(port_b), _last_porta(0), _last_portb(0) {}
};

PARAM_HANDLER(scp,
	      "scp:porta,portb - provide the system control ports A+B.",
	      "Example: 'scp:0x92,0x61'")
{
  SystemControlPort *scp = new SystemControlPort(mb.bus_legacy, mb.bus_pit, argv[0], argv[1]);
  mb.bus_ioin.add(scp,  SystemControlPort::receive_static<MessageIOIn>);
  mb.bus_ioout.add(scp, SystemControlPort::receive_static<MessageIOOut>);
}
