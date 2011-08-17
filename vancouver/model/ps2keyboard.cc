/** @file
 * PS2keyboard emulation.
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
#include "host/keyboard.h"

/**
 * A PS2 keyboard gets characters on the hostbus as input and outputs
 * scancodes on the ps2 bus.
 *
 * State: stable
 * Features: scancodeset 1,2,3, keyboard commands, breakcode
 * Missing:  typematic keys
 * Documentation: PS2 hitrc chapter 11
 */
class PS2Keyboard : public StaticReceiver<PS2Keyboard>
{
  DBus<MessagePS2> &_bus_ps2;
  unsigned _ps2port;
  unsigned _hostkeyboard;
  unsigned char _scset;
  static const unsigned BUFFERSIZE = 18;
  unsigned char _buffer[BUFFERSIZE];
  unsigned _pread;
  unsigned _pwrite;
  unsigned _response;
  unsigned char _no_breakcode[32];
  unsigned char _indicators;
  unsigned char _last_command;
  unsigned char _last_reply;
  enum {
    MODE_DISABLED = 1 << 0,
    MODE_RESET    = 1 << 1,
    MODE_RESEND   = 1 << 2,
    MODE_GOT_BREAK= 1 << 3,
    MODE_STOPPED  = 1 << 4,
  };
  unsigned char _mode;


  /**
   * Enqueue a single scancode in the buffer.
   * Please note that the values are translated if SCS1 is used.
   */
  void enqueue(unsigned char value)
  {
    if (_scset == 1)
      {
	if (value == 0xf0)
	  {
	    _mode |= MODE_GOT_BREAK;
	    return;
	  }
	if (_mode & MODE_GOT_BREAK) value |= 0x80;
	_mode &= ~MODE_GOT_BREAK;
	value = GenericKeyboard::translate_sc2_to_sc1(value);
      }
    switch ((_pwrite - _pread) % BUFFERSIZE)
      {
      case 0 ... BUFFERSIZE-3:
	_buffer[_pwrite] = value;
	break;
      case BUFFERSIZE-2:
	// full
	_buffer[_pwrite] = 0x00;
	break;
      default:
      case BUFFERSIZE-1:
	return;
      }
    _pwrite = (_pwrite+1) % BUFFERSIZE;
  }

  /**
   * Enqueue a whole set of characters.
   */
  void enqueue_string(const char *value)
  {
    while (*value)  enqueue(*value++);
  }

  /**
   * Reset the keyboard to its default state.
   */
  void reset()
  {
    // we use scset 2 here as we have translation on in the keyboard controller
    _scset = 2;
    _pread = 0;
    _pwrite = 0;
    _response = 0;
    memset(_no_breakcode, 0, sizeof(_no_breakcode));
    memcpy(_no_breakcode, "\x80\x81\x80\x80\x80\x80\x80\x82\x80\x80\xc0\xc1\xa4\xfa\xff\x66\x10", 17);
    _indicators = 0;
    _last_command = 0;
    _last_reply = 0;
    _mode = 0;
  }


  /**
   * Returns whether a keycode respects a shift modifier.
   */
  bool has_shift_modifiers(unsigned char key)
  {
    switch (key)
      {
      case 0x69:
      case 0x6b ... 0x6c:
      case 0x70 ... 0x72:
      case 0x74 ... 0x75:
      case 0x7a:
      case 0x7d:
      case 0x7c:
      case 0x4a:
	return true;
      default:
	return false;
      }
  }


  /**
   * Enqueues corresponding shift release or press events for the
   * shift-keys if the shift-modifier is changed for the key.
   */
  void handle_shift_modifiers(unsigned value)
  {
    if (value & KBFLAG_EXTEND0 && has_shift_modifiers(value))
      {
	unsigned char key = value;
	unsigned modifiers = 0;
	if (value & KBFLAG_LSHIFT && (key == 0x4a || ~value & KBFLAG_NUM))
	  modifiers = 0x12;
	if (value & KBFLAG_RSHIFT && (key == 0x4a || ~value & KBFLAG_NUM))
	  modifiers = value & KBFLAG_RELEASE ? 0x59 | (modifiers << 8) : 0x5900 | modifiers;
	if (key == 0x7c && !(value & (KBFLAG_RSHIFT | KBFLAG_LSHIFT | KBFLAG_RCTRL | KBFLAG_LCTRL))
	    || !(value & (KBFLAG_RSHIFT | KBFLAG_LSHIFT)) && value & KBFLAG_NUM && key != 0x4a)
	  {
	    value ^= KBFLAG_RELEASE;
	    modifiers = 0x12;
	  }
	while (modifiers)
	  {
	    enqueue(0xe0);
	    if (~value & KBFLAG_RELEASE) enqueue(0xf0);
	    enqueue(modifiers);
	    modifiers >>= 8;
	  }
      }
  }

 public:

  bool  receive(MessageInput &msg)
  {
    if (msg.device != _hostkeyboard)  return false;

    if (_mode & (MODE_DISABLED | MODE_STOPPED))
      return false;

    unsigned oldwrite = _pwrite;
    if (_scset == 2 || _scset == 1)
      {
	unsigned char key = msg.data;
	_mode &= ~MODE_GOT_BREAK;
	if (msg.data & KBFLAG_EXTEND1)
	  {
	    enqueue(0xe1);
	    if (key == 0x77)	        // the pause key ?
	      enqueue_string("\x14\x77\xe1\xf0\x14");
	  }
	if (key == 0x7e && msg.data & KBFLAG_EXTEND0 && msg.data & KBFLAG_RELEASE)
	  enqueue_string("\xe0\0x7e"); 	// sysctrl key

	if (~msg.data & KBFLAG_RELEASE)  handle_shift_modifiers(msg.data);
	if (msg.data & KBFLAG_EXTEND0)   enqueue(0xe0);
	if (msg.data & KBFLAG_RELEASE)   enqueue(0xf0);
	enqueue(msg.data);
	if (msg.data & KBFLAG_RELEASE)   handle_shift_modifiers(msg.data);

#if 0
	if ((_pwrite - _pread) % BUFFERSIZE == BUFFERSIZE - 1)
	  {
	    _pwrite = oldwrite;
	    enqueue(0x00);
	  }
#endif
      }
    else // _scset == 3
      {
	unsigned char key = GenericKeyboard::translate_sc2_to_sc3(msg.data);

	// the pause key sends make and break together -> simulate another make
	if (key == 0x62)  enqueue(key);
	if (msg.data & KBFLAG_RELEASE) {

	  if (_no_breakcode[key >> 3] & (1 << (key & 7)))
	    return true;
	  enqueue(0xf0);
	}
	enqueue(key);
      }

    if (oldwrite == _pread) {

      MessagePS2 msg2(_ps2port, MessagePS2::NOTIFY, 0);
      _bus_ps2.send(msg2);
    }

    return true;
  }


  bool  receive(MessagePS2 &msg)
  {
    if (msg.port != _ps2port) return false;
    if (msg.type == MessagePS2::READ_KEY)
      {
	if (_mode & MODE_RESEND)
	  {
	    msg.value = _last_reply;
	    _mode &= ~MODE_RESEND;
	  }
	else if (_response)
	  {
	    msg.value = _response & 0xff;
	    _response >>= 8;
	    if (_mode & MODE_RESET)
	      {
		assert(!_response && msg.value == 0xfa);
		reset();
		_response = 0xaa;
		_mode &= ~MODE_RESET;
	      }
	  }
	else
	  {
	    msg.value = _buffer[_pread];
	    if (_pwrite != _pread)
	      _pread = (_pread + 1) % BUFFERSIZE;
	    else
	      return false;
	  }
	_last_reply = msg.value;
      }
    else if (msg.type == MessagePS2::SEND_COMMAND)
      {
	unsigned new_response = 0xfa;
	unsigned char command = msg.value;
	unsigned char new_mode = _mode & ~(MODE_STOPPED | MODE_RESET);
	switch (msg.value)
	  {
	  case 0xf0: // set scanset
	    _pwrite = _pread = 0;
	  case 0xed: // set indicators
	  case 0xf3: // set typematic rate/delay
	  case 0xfb: // set key type typematic
	  case 0xfc: // set key type make+break
	  case 0xfd: // set key type make
	    new_mode |= MODE_STOPPED;
	    break;
	  case 0xee: // echo
	    new_response = 0xee;
	    break;
	  case 0xf2: // read ID
	    new_response = 0x83abfa;
	    break;
	  case 0xf4: // enable
	    _pwrite = _pread = 0;
	    new_mode &= ~MODE_DISABLED;
	    break;
	  case 0xf5: // default+disable
	    reset();
	    new_mode |= MODE_DISABLED;
	    break;
	  case 0xf6: // default+enabled
	    reset();
	    new_mode &= ~MODE_DISABLED;
	    break;
	  case 0xf8: // all keys make-break
	  case 0xfa: // all keys make-break+typematic
	    memset(_no_breakcode, 0, sizeof(_no_breakcode));
	    break;
	  case 0xf9: // all keys make
	    memset(_no_breakcode, 0xff, sizeof(_no_breakcode));
	  case 0xf7: // all keys typematic
	    break;
	  case 0xfe:
	    new_mode |= MODE_RESEND;
	    new_response = 0;
	    break;
	  case 0xff:
	    new_mode |= MODE_RESET | MODE_STOPPED;
	    break;
	  default:
	    switch (_last_command)
	      {
	      case 0xed: // set indicators
		_indicators = msg.value;
	      case 0xf3: // set typematic rate/delay
		command = 0;
		break;
	      case 0xf0: // set scanset
		switch (msg.value)
		  {
		  case 0:
		    new_response = _scset << 16 | 0xfa;
		    break;
		  case 1 ... 3:
		    _scset = msg.value;
		    new_response = 0xfa;
		    break;
		  default:
		    new_response = 0xff;
		  }
		command = 0;
		break;
	      case 0xfc: // set key type make+break
	      case 0xfd: // set key type make
		if (_last_command == 0xfc)
		  _no_breakcode[msg.value >> 3] &= ~(1 << (msg.value & 7));
		else
		  _no_breakcode[msg.value >> 3] |= 1 << (msg.value & 7);
	      case 0xfb: // set key type typematic
		if (msg.value)
		  command = _last_command;
		else
		  new_response = 0xfe;
		break;
	      default:
		new_response = 0xfe;
		command = 0;
		break;
	      }
	    if (command)  new_mode |= MODE_STOPPED;
	  }
	_last_command = command;
	_mode = new_mode;
	if (new_response)  _response = new_response;
	if (_response || _mode & MODE_RESEND)
	  {
	    MessagePS2 msg2(_ps2port, MessagePS2::NOTIFY, 0);
	    _bus_ps2.send(msg2);
	  }
      }
    else  return false;
    return true;
  }

  bool  receive(MessageLegacy &msg) {
    if (msg.type != MessageLegacy::RESET) return false;
    reset();
    return true;
  }

 PS2Keyboard(DBus<MessagePS2>  &bus_ps2, unsigned ps2port, unsigned hostkeyboard)
   : _bus_ps2(bus_ps2), _ps2port(ps2port), _hostkeyboard(hostkeyboard)
  {}
};

PARAM_HANDLER(keyb,
	      "keyb:ps2port,hostkeyboard - attach a PS2 keyboard at the given PS2 port that gets input from the given hostkeyboard.",
	      "Example: 'keyb:0,0x17'")
{
  PS2Keyboard *dev = new PS2Keyboard(mb.bus_ps2, argv[0], argv[1]);
  mb.bus_ps2.add(dev,   PS2Keyboard::receive_static<MessagePS2>);
  mb.bus_input.add(dev, PS2Keyboard::receive_static<MessageInput>);
  mb.bus_legacy.add(dev,PS2Keyboard::receive_static<MessageLegacy>);
}

