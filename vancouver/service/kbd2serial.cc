/**
 * Converts keystrokes to ascii chars.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <bk@vmmon.org>
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
#include "models/keyboard.h"
#include "vmm/motherboard.h"


/**
 * A bridge on the keycode bus that converts keystrokes to ascii
 * chars on the serial bus.
 *
 * State: testing
 * Features: alphanumeric chars, ANSI escape codes
 * Missing: some special chars, see keyboard.h for details.
 */
class KbdSerialBridge : public StaticReceiver<KbdSerialBridge>
{
  DBus<MessageSerial> &_bus_serial;
  unsigned _keyboard;
  unsigned _serial;
  const char *debug_getname() { return "KbdSerialBridge"; };

  const char *keycode2ansi(unsigned value)
  {
    GenericKeyboard::ansi_map *ansi_map = GenericKeyboard::get_ansi_map();
    for (unsigned i=0; ansi_map[i].keycode; i++)
      if (ansi_map[i].keycode == value)
	return ansi_map[i].escape;
    return 0;
  }


  char keycode2ascii(unsigned value)
  {
    unsigned *ascii_map = GenericKeyboard::get_ascii_map();
    for (unsigned i=0; i < 128; i++)
      if (ascii_map[i] == value)
	return i;
    return 0;
  }


  void send_char(char ch)
  {
    MessageSerial msg(_serial, ch);
    _bus_serial.send(msg);
  }

public:
  bool  receive(MessageKeycode &msg)
  {
    if (msg.keyboard != _keyboard)  return false;

    msg.keycode &= ~KBFLAG_NUM;
    if (msg.keycode & (KBFLAG_RSHIFT | KBFLAG_LSHIFT))
      msg.keycode = msg.keycode & ~(KBFLAG_RSHIFT | KBFLAG_LSHIFT) | KBFLAG_LSHIFT;
    const char *s = keycode2ansi(msg.keycode);
    if (s)
      {
	send_char(0x1b);
	while (s && *s)  send_char(*s++);
      }
    else
      {
	if ((msg.keycode = keycode2ascii(msg.keycode)))
	  send_char(msg.keycode);
      }
    return true;
  }


  KbdSerialBridge(DBus<MessageSerial> &bus_serial, unsigned keyboard, unsigned serial) : _bus_serial(bus_serial), _keyboard(keyboard), _serial(serial) {}
};


PARAM(kbd2serial,
      {
	Device *dev = new KbdSerialBridge(mb.bus_serial, argv[0], argv[1]);
	mb.bus_keycode.add(dev, &KbdSerialBridge::receive_static<MessageKeycode>);
      },
      "kbd2serial:src,dst - attach a bridge between keyboard and keyboard.",
      "Example: 'kbd2serial:0x2bad,0x4711'.",
      "The keystrokes at src hostdevice are transformed to serial chars at the dest hostdev.");
