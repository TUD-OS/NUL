/** @file
 * PS2 keyboard controller emulation.
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
 * A PS2 keyboard controller.
 *
 * State: stable
 * Features: scancode transfer, translation, cmdbytes, A20, reset, pwd
 * Open: transfer NUM-lock state changes upstream
 * Documentation: PS2 hitrc chapter 7
 */
class KeyboardController : public StaticReceiver<KeyboardController>
{
  enum
  {
    STATUS_OBF    = 1 << 0,
    STATUS_SYS    = 1 << 2,
    STATUS_CMD    = 1 << 3,
    STATUS_NO_INHB= 1 << 4,
    STATUS_AUX    = 1 << 5,
    STATUS_AUXOBF = STATUS_AUX | STATUS_OBF,
    CMD_IRQKBD    = 1 << 0,
    CMD_IRQAUX    = 1 << 1,
    CMD_SYS       = STATUS_SYS,
    CMD_DISKBD    = 1 << 4,
    CMD_DISAUX    = 1 << 5,
    CMD_TRANSLATE = 1 << 6,
    OUTPORT_RESET = 1 << 0,
    OUTPORT_A20   = 1 << 1,
    OUTPORT_IRQKBD= 1 << 4,
    OUTPORT_IRQAUX= 1 << 5,
    RAM_CMDBYTE   = 0x00,
    RAM_STATUS    = 0x01,
    RAM_OBF       = 0x02,
    RAM_LASTCMD   = 0x03,
    RAM_GOT_RELEASE= 0x04,
    RAM_OUTPORT   = 0x05,
    RAM_PWD_COUNT = 0x06,
    RAM_PWD_CMP   = 0x07,
    RAM_PWD_FIRST = 0x08,
    RAM_PWD_LAST  = 0x0e,
    RAM_SECON     = 0x13,
    RAM_SECOFF    = 0x14,
    RAM_MAKE1     = 0x16,
    RAM_MAKE2     = 0x17,
    RAM_LOCK      = 0x18,
  };

  DBus<MessageIrqLines> &_bus_irqlines;
  DBus<MessagePS2>	&_bus_ps2;
  DBus<MessageLegacy>   &_bus_legacy;
  unsigned short _base;
  unsigned _irqkbd;
  unsigned _irqaux;
  unsigned _ps2ports;
  unsigned char _ram[32];

  unsigned char translate_keycodes(unsigned char value) {
    if (_ram[RAM_CMDBYTE] & CMD_TRANSLATE)
      {
	if (value == 0xf0)
	  {
	    _ram[RAM_GOT_RELEASE] = true;
	    return value;
	  }
	value = GenericKeyboard::translate_sc2_to_sc1(value);
	if (_ram[RAM_GOT_RELEASE]) value |= 0x80;
      }
    _ram[RAM_GOT_RELEASE] = false;
    return value;
  }

  void read_from_device(unsigned char port)
  {
    while (~_ram[RAM_STATUS] & STATUS_OBF)
      {
	MessagePS2 msg(port, MessagePS2::READ_KEY, 0);
	if (!_bus_ps2.send(msg))
	  return;

	if (port == _ps2ports) {
	  msg.value = translate_keycodes(msg.value);
	  if (_ram[RAM_GOT_RELEASE]) continue;
	}
	got_data(msg.value, port != _ps2ports);
      }
  }


  void read_all_devices() {
    if (_ram[RAM_LOCK]) return;
    _ram[RAM_LOCK] = 1;
    if (~_ram[RAM_CMDBYTE] & CMD_DISAUX)  read_from_device(_ps2ports + 1);
    if (~_ram[RAM_CMDBYTE] & CMD_DISKBD)  read_from_device(_ps2ports);
    _ram[RAM_LOCK] = 0;
  }


  bool check_pwd(unsigned char &value, bool from_aux)
  {
    if (~_ram[RAM_STATUS] & STATUS_NO_INHB)
      {
	if (value >= 0x80 || value == _ram[RAM_MAKE1] || value == _ram[RAM_MAKE2] || from_aux)
	  return true;
	if (value == _ram[RAM_PWD_FIRST + _ram[RAM_PWD_CMP]])
	  _ram[RAM_PWD_CMP]++;
	else
	  _ram[RAM_PWD_CMP] = 0;
	if (_ram[RAM_PWD_CMP] > RAM_PWD_LAST - RAM_PWD_FIRST || !_ram[RAM_PWD_FIRST + _ram[RAM_PWD_CMP]])
	  {
	    _ram[RAM_STATUS] |= STATUS_NO_INHB;
	    if (!_ram[RAM_SECOFF])
	      {
		value = _ram[RAM_SECOFF];
		return false;
	      }
	  }
	return true;
      }
    return false;
  }


  void got_data(unsigned char value, bool from_aux)
  {
    if (check_pwd(value, from_aux))  return;

    _ram[RAM_OBF] = value;
    _ram[RAM_STATUS] = _ram[RAM_STATUS] & ~STATUS_AUXOBF | (from_aux ? STATUS_AUXOBF : STATUS_OBF);

    if ((_ram[RAM_STATUS] & STATUS_AUXOBF) == STATUS_AUXOBF   && _ram[RAM_CMDBYTE] & CMD_IRQAUX)
      {
	_ram[RAM_OUTPORT] |= OUTPORT_IRQAUX;
	MessageIrqLines msg(MessageIrq::ASSERT_IRQ, _irqaux);
	_bus_irqlines.send(msg);
      }
    else if ((_ram[RAM_STATUS] & STATUS_AUXOBF) == STATUS_OBF && _ram[RAM_CMDBYTE] & CMD_IRQKBD)
      {
	_ram[RAM_OUTPORT] |= OUTPORT_IRQKBD;
	MessageIrqLines msg(MessageIrq::ASSERT_IRQ, _irqkbd);
	_bus_irqlines.send(msg);
      }
  }


  void legacy_write(MessageLegacy::Type type, unsigned value)
  {
    MessageLegacy msg(type, value);
    _bus_legacy.send_fifo(msg);
  };

public:

  bool  receive(MessageIOIn &msg)
  {
    if (msg.type != MessageIOIn::TYPE_INB) return false;
    if (msg.port == _base)
      {
	msg.value = _ram[RAM_OBF];
	_ram[RAM_STATUS] &= ~STATUS_AUXOBF;
	_ram[RAM_OUTPORT] &= ~(OUTPORT_IRQAUX | OUTPORT_IRQKBD);
	read_all_devices();
      }
    else if (msg.port == _base + 4)
	msg.value = _ram[RAM_STATUS] & ~STATUS_SYS | _ram[RAM_CMDBYTE] & CMD_SYS;
    else
      return false;
    return true;
  }


  bool  receive(MessageIOOut &msg)
  {
    if (msg.type != MessageIOOut::TYPE_OUTB) return false;
    if (msg.port == _base)
      {
	if (~_ram[RAM_STATUS] & STATUS_NO_INHB)  return true;
	bool handled = false;
	if (_ram[RAM_STATUS] & STATUS_CMD)
	  {
	    handled = true;
	    switch (_ram[RAM_LASTCMD])
	      {
	      case 0x60 ... 0x7f: // write ram
		_ram[_ram[RAM_LASTCMD] - 0x60] = msg.value;
		break;
	      case 0xa5: // load pwd
		if (_ram[RAM_PWD_COUNT] + RAM_PWD_FIRST <= RAM_PWD_LAST)
		  {
		    _ram[_ram[RAM_PWD_COUNT] + RAM_PWD_FIRST] = msg.value;
		    _ram[RAM_PWD_COUNT]++;
		    if (!msg.value) _ram[RAM_PWD_COUNT] = RAM_PWD_LAST + 1;
		  }
		break;
	      case 0xd1: // write outport
		_ram[RAM_OUTPORT] = msg.value;
		legacy_write(MessageLegacy::GATE_A20, _ram[RAM_OUTPORT] & OUTPORT_A20 ? 1 : 0);
		if (~_ram[RAM_OUTPORT] & OUTPORT_RESET)
		  legacy_write(MessageLegacy::RESET, 0);
		break;
	      case 0xd2: // write keyboard output buffer
		got_data(translate_keycodes(msg.value), false);
		break;
	      case 0xd3: // write aux output buffer
		got_data(msg.value, true);
		break;
	      case 0xd4: // forward to aux
		{
		  MessagePS2 msg2(_ps2ports + 1, MessagePS2::SEND_COMMAND, msg.value);
		  _bus_ps2.send(msg2);
		}
		break;
	      case 0xdd: // disable a20
		_ram[RAM_OUTPORT] &= ~OUTPORT_A20;
		legacy_write(MessageLegacy::GATE_A20, _ram[RAM_OUTPORT] & OUTPORT_A20 ? 1 : 0);
		break;
	      case 0xdf: // enable a20
		_ram[RAM_OUTPORT] |= OUTPORT_A20;
		legacy_write(MessageLegacy::GATE_A20, _ram[RAM_OUTPORT] & OUTPORT_A20 ? 1 : 0);
		break;
	      default:
		handled = false;
		break;
	      }
	  }
	_ram[RAM_STATUS] &= ~STATUS_CMD;
	if (!handled)
	  {
	    MessagePS2 msg2(_ps2ports, MessagePS2::SEND_COMMAND, msg.value);
	    _bus_ps2.send(msg2);
	  }
      }
    else if (msg.port == _base+4)
      {
	if (~_ram[RAM_STATUS] & STATUS_NO_INHB)  return true;
	_ram[RAM_LASTCMD] = msg.value;
	_ram[RAM_STATUS] |= STATUS_CMD;
	switch (_ram[RAM_LASTCMD])
	  {
	  case 0x20 ... 0x3f: // read ram
	    got_data(_ram[_ram[RAM_LASTCMD] - 0x20], false);
	    break;
	  case 0xa4: // pwd installed ?
	    got_data(_ram[RAM_PWD_COUNT] ? 0xfa : 0xf1, false);
	    break;
	  case 0xa5: // load pwd
	    break;
	  case 0xa6: // enable pwd
	    _ram[RAM_STATUS] &= ~STATUS_NO_INHB;
	    _ram[RAM_PWD_CMP] = 0;
	    if (_ram[RAM_SECON])
	      {
		_ram[RAM_OBF] = _ram[RAM_SECON];
		_ram[RAM_STATUS] = _ram[RAM_STATUS] & ~STATUS_AUXOBF | STATUS_OBF;
		if (_ram[RAM_CMDBYTE] & CMD_IRQKBD)
		  {
		    MessageIrqLines msg2(MessageIrq::ASSERT_IRQ, _irqkbd);
		    _bus_irqlines.send(msg2);
		  }
	      }
	    break;
	  case 0xa7: // disable aux
	    _ram[RAM_CMDBYTE] |= CMD_DISAUX;
	    break;
	  case 0xa8: // enable kbd
	    _ram[RAM_CMDBYTE] &= ~CMD_DISAUX;
	    break;
	  case 0xa9: // aux interface test
	    got_data(0, false);
	    break;
	  case 0xaa: // self test
	    got_data(0x55, false);
	    break;
	  case 0xab: // kbd interface test
	    got_data(0, false);
	    break;
	  case 0xad: // disable kbd
	    _ram[RAM_CMDBYTE] |= CMD_DISKBD;
	    break;
	  case 0xae: // enable kbd
	    _ram[RAM_CMDBYTE] &= ~CMD_DISKBD;
	    break;
	  case 0xc0: // read input port
	    got_data(0, false);
	    break;
	  case 0xd0: // read output port
	    got_data(_ram[RAM_OUTPORT], false);
	    break;
	  case 0xe0: // read test port
	    got_data(0, false);
	    break;
	  case 0xf0 ... 0xff:
	    if (~msg.value & 0x1) legacy_write(MessageLegacy::RESET, 0);
	    break;
	  default:
	    break;
	  }
      }
    else
      return false;
    return true;
  }

  bool  receive(MessagePS2 &msg)
  {
    if (!in_range(msg.port, _ps2ports, 2) || msg.type != MessagePS2::NOTIFY)   return false;
    read_all_devices();
    return true;
  }


  bool  receive(MessageLegacy &msg)
  {
    if (msg.type == MessageLegacy::RESET)
      {
	memset(_ram, 0, sizeof(_ram));
	_ram[RAM_CMDBYTE] = CMD_IRQKBD |  CMD_TRANSLATE;
	_ram[RAM_STATUS]  = STATUS_NO_INHB;
	_ram[RAM_OUTPORT] = OUTPORT_RESET | OUTPORT_A20;
      }
    return false;
  }

  KeyboardController(DBus<MessageIrqLines> &bus_irqlines, DBus<MessagePS2> &bus_ps2, DBus<MessageLegacy> &bus_legacy,
		     unsigned short base, unsigned irqkbd, unsigned irqaux, unsigned ps2ports)
   : _bus_irqlines(bus_irqlines), _bus_ps2(bus_ps2), _bus_legacy(bus_legacy), _base(base), _irqkbd(irqkbd), _irqaux(irqaux), _ps2ports(ps2ports)
  {}
};

PARAM_HANDLER(kbc,
	      "kbc:iobase,irqkeyb,irqaux - attach an PS2 keyboard controller at the given iobase.",
	      "Example: 'kbc:0x60,1,12'",
	      "The PS2 ports are automatically distributed, such that the first KBC gets 0-1, the second one 2-3,...")
{
  static unsigned kbc_count;
  KeyboardController *dev = new KeyboardController(mb.bus_irqlines, mb.bus_ps2, mb.bus_legacy, argv[0], argv[1], argv[2], 2*kbc_count++);
  mb.bus_ioin.add(dev,  KeyboardController::receive_static<MessageIOIn>);
  mb.bus_ioout.add(dev, KeyboardController::receive_static<MessageIOOut>);
  mb.bus_ps2.add(dev,   KeyboardController::receive_static<MessagePS2>);
  mb.bus_legacy.add(dev,KeyboardController::receive_static<MessageLegacy>);
}

