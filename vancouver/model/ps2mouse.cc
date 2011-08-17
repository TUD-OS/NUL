/** @file
 * PS2Mouse emulation.
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
 * A PS2 keyboard gets mouse packets the hostbus as input and outputs
 * them on the ps2 bus.
 *
 * State: stable
 * Features: mouse commands including poll mode, scaling, packet merging, resolution adaption
 * Missing:  different sample rate, packet resend, z-coordinate
 * Documentation: PS2 hitrc, PS2 Mouse Interface, Trackpoint Engineering Specification 3E
 */
class PS2Mouse : public StaticReceiver<PS2Mouse>
{
  unsigned static const HOST_RESOLUTION_SHIFT = 2;
  DBus<MessagePS2> &_bus_ps2;
  unsigned _ps2port;
  unsigned _hostmouse;
  unsigned long long _packet;
  enum
  {
    STATUS_RIGHT       = 1 << 0,
    STATUS_MIDDLE      = 1 << 1,
    STATUS_LEFT        = 1 << 2,
    STATUS_SCALING     = 1 << 4,
    STATUS_ENABLED     = 1 << 5,
    STATUS_REMOTE      = 1 << 6,
  };
  unsigned char _status;
  unsigned char _resolution;
  unsigned char _samplerate;
  int _posx;
  int _posy;
  enum Params
  {
    PARAM_NONE,
    PARAM_ECHO,
    PARAM_SET_RESOLUTION,
    PARAM_SET_SAMPLERATE,
  } _param;


  int scale_coord(bool report, int value)
  {
    if (_resolution < HOST_RESOLUTION_SHIFT)
      value >>=  HOST_RESOLUTION_SHIFT - _resolution;
    else
      value <<= -HOST_RESOLUTION_SHIFT + _resolution;

    if (report && _status & STATUS_SCALING)
      {
	switch (value)
	  {
	  case 0:
	  case -1:
	  case 1:
	    break;
	  case -2:
	  case 2:
	    value >>= 1;
	    break;
	  case 3:
	  case 4:
	  case 5:
	    value = (value - 2)*3;
	    break;
	  case -3:
	  case -4:
	  case -5:
	    value = (value + 2)*3;
	    break;
	  default:
	    value *= 2;
	    break;
	  }
      }
    return value;
  };


  /**
   * Generate a packet and update the stored positions
   */
  unsigned gen_packet(bool report)
  {
    unsigned value;

    _posx = scale_coord(report, _posx);
    _posy = scale_coord(report, _posy);
    bool negx= _posx < 0;
    bool negy= _posy < 0;
    bool ovx = _posx > 255 || _posx < -256;
    bool ovy = _posy > 255 || _posy < -256;

    // correctly report overflows
    value = (ovy ? 0x8000 : 0) | (ovx ? 0x4000 : 0) | (negy ? 0x2000 : 0) | (negx ? 0x1000 : 0) | ((_status & 0xf) << 8) | 0x3;

    // upper limit movements
    _posx = ovx ? (negx ? -256 : 255) : _posx;
    _posy = ovy ? (negy ? -256 : 255) : _posy;

    // calc values
    value |= (_posx & 0xff) << 16;
    value |= (_posy & 0xff) << 24;

    // the movement counters are reset when getting a packet
    _posx = 0;
    _posy = 0;
    return value;
  }


  void set_packet(unsigned long long packet)
  {
    _packet = packet;
    MessagePS2 msg2(_ps2port, MessagePS2::NOTIFY, 0);
    _bus_ps2.send(msg2);
  }


  void update_packet()
  {
    if (!(_packet & 0xff) && ~_status & STATUS_REMOTE)
      set_packet(gen_packet(true));
  }


 public:
  bool  receive(MessageInput &msg)
  {
    if (msg.device != _hostmouse)
      return false;

    // we support only 3 byte packets
    assert((msg.data & 0xff) == 3);

    // we ignore the overflow bit as everybody does
    _posx += ((msg.data >> 16) & 0xff) - ((msg.data >> 4) & 0x100);
    _posy += ((msg.data >> 24) & 0xff) - ((msg.data >> 5) & 0x100);
    _status = _status & 0xf8 | (msg.data >> 8) & 0x7;
    update_packet();

    return true;
  }


  bool  receive(MessagePS2 &msg)
  {
    if (msg.port != _ps2port)  return false;
    bool res = false;
    if (msg.type == MessagePS2::READ_KEY)
      {
	switch (_packet & 0xff)
	  {
	  case 1 ... 3:
	    msg.value = (_packet >> 8) & 0xff;
	    _packet = ((_packet >> 8) & ~0xffULL)  | ((_packet & 0xff) - 1);
	    res = true;
	  default:
	    break;
	  }
      }
    else if (msg.type == MessagePS2::SEND_COMMAND)
      {
	res = true;
	unsigned long long packet = 0;
	switch (_param)
	  {
	  case PARAM_SET_RESOLUTION:
	    _resolution = msg.value & 0x3;
	    _param = PARAM_NONE;
	    packet = 0xfa01;
	    break;
	  case PARAM_SET_SAMPLERATE:
	    // we do not check for magic sequences and odd values
	    _samplerate = msg.value;
	    _param = PARAM_NONE;
	    packet = 0xfa01;
	    break;
	  case PARAM_ECHO:
	    if (msg.value != 0xff && msg.value != 0xec)
	      {
		packet = msg.value << 8 | 1;
		break;
	      }
	  default:
	  case PARAM_NONE:
	    packet = 0xfa01;
	    switch (msg.value & 0xff)
	      {
	      case 0xe6:  // set mouse scaling 1:1
		_status &= ~STATUS_SCALING;
		break;
	      case 0xe7:  // set mouse scaling 1:2
		_status |= STATUS_SCALING;
		break;
	      case 0xe8: // set resolution
		_param = PARAM_SET_RESOLUTION;
		break;
	      case 0xe9: // status request
		packet = union64(_samplerate, _resolution << 24 | _status << 16 | 0xfa00 | 4);
		break;
	      case 0xea: // set stream mode
		_status &= ~STATUS_REMOTE;
		break;
	      case 0xeb: // read packet
		packet = gen_packet(false);
		packet = (packet & ~0xff) << 16 | 0xfa00;
		packet++;
		break;
	      case 0xec: // clear echo mode
		_param = PARAM_NONE;
		break;
	      case 0xee: // echo mode
		_param = PARAM_ECHO;
		break;
	      case 0xf0: // set remote mode
		_status |= STATUS_REMOTE;
		break;
	      case 0xf2: // read id
		packet = 0x00fa02;
		break;
	      case 0xf3: // set sample rate
		_param = PARAM_SET_SAMPLERATE;
		break;
	      case 0xf4: // enable mouse
		_status |= STATUS_ENABLED;
		break;
	      case 0xf5: // disable mouse
		_status &= ~STATUS_ENABLED;
		break;
	      case 0xf6: // set default
		set_defaults();
		packet = 0xfa01;
		break;
	      case 0xff: // reset
		set_defaults();
		packet = 0x00aafa03;
		break;
	      default:
		Logging::printf("%s(%x, %x) unknown command\n", __PRETTY_FUNCTION__, msg.port, msg.value);
	      case 0xe1: // read secondary ID - used to identify trackpoints
		packet = 0xfc01;
	      }
	    break;
	  }
	if (packet & 0xff)  set_packet(packet);
      }
    return res;
  }


  void set_defaults()
  {
    _packet = 0;
    _samplerate = 100;
    _resolution = 2;
    _status = 0x8;
    _param = PARAM_NONE;
    _posx = 0;
    _posy = 0;
  };


  PS2Mouse(DBus<MessagePS2> &bus_ps2, unsigned ps2port, unsigned hostmouse) : _bus_ps2(bus_ps2), _ps2port(ps2port), _hostmouse(hostmouse)
  {
    set_defaults();
  }
};

PARAM_HANDLER(mouse,
	      "mouse:ps2port,hostmouse:  attach a PS2 mouse at the given PS2 port that gets input from the given hostmouse.",
	      "Example: 'mouse:1,0x17'")
{
  PS2Mouse *dev = new PS2Mouse(mb.bus_ps2, argv[0], argv[1]);
  mb.bus_ps2.add(dev,   PS2Mouse::receive_static<MessagePS2>);
  mb.bus_input.add(dev, PS2Mouse::receive_static<MessageInput>);
}

