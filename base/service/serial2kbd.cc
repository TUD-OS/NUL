/** @file
 * Converts ascii chars to keystrokes.
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
#include "host/keyboard.h"
#include "nul/motherboard.h"

/**
 * A bridge on the host bus that converts serial chars to keystrokes.
 *
 * State: testing
 * Features: alphanumeric chars, ANSI escape codes
 * Missing: some special chars, see keyboard.h for details.
 */
class SerialKbdBridge : public StaticReceiver<SerialKbdBridge>
{
  DBus<MessageInput> &_bus_input;
  unsigned _serial;
  unsigned _keyboard;
  unsigned char _escape;
  unsigned char _escape_chars[5];


  /**
   * Translate ascii code to SCS2 scancodes for an US keyboard layout.
   */
  unsigned translate_ascii(unsigned char key)
  {
    unsigned *ascii_map = GenericKeyboard::get_ascii_map();
    unsigned res = 0;
    if (key < 128)
      res = ascii_map[key];
    return res;
  }


  /**
   * Translate ansi escape sequences
   */
  unsigned translate_ansi(unsigned char key)
  {
    _escape_chars[_escape++] = key;
    GenericKeyboard::ansi_map *ansi_map = GenericKeyboard::get_ansi_map();
    for (unsigned i=0; ansi_map[i].keycode; i++)
      {
	unsigned len = strlen(ansi_map[i].escape);
	if (len + 1 == _escape && !memcmp(_escape_chars+1, ansi_map[i].escape, len))
	  {
	    _escape = 0;
	    return ansi_map[i].keycode;
	  }
      }
    // if we have more than 4 escape keys stored -> drop them to avoid overflow
    if (_escape > 4) _escape = 0;
    return 0;
  }


  void send_key(unsigned keycode)
  {
    MessageInput msg(_keyboard, keycode);
    _bus_input.send(msg);
  }

public:
  bool  receive(MessageSerial &msg)
  {
    if (msg.serial != _serial)   return false;

    unsigned keycode;
    if (msg.ch == 0x1b || _escape)
      keycode = translate_ansi(msg.ch);
    else
      keycode = translate_ascii(msg.ch);

    if (keycode)
      {
	if (keycode & KBFLAG_LSHIFT) send_key(0x12);
	send_key(keycode);
	send_key(KBFLAG_RELEASE | keycode);
	if (keycode & KBFLAG_LSHIFT)
	  send_key(KBFLAG_RELEASE | 0x12);
      }
    return true;
  }


  SerialKbdBridge(DBus<MessageInput> &bus_input, unsigned serial, unsigned keyboard) : _bus_input(bus_input), _serial(serial), _keyboard(keyboard), _escape(0) {}
};


PARAM_HANDLER(serial2kbd,
	      "serial2kbd:serial,keyboard - attach a bridge between serial and keyboard.",
	      "Example: 'serial2kbd:0x4711,0x2bad'.",
	      "The serial input at source serialbus is transformed to keystrokes on the dest keycodebus.")
{
  mb.bus_serial.add(new SerialKbdBridge(mb.bus_input, argv[0], argv[1]), SerialKbdBridge::receive_static<MessageSerial>);
}
