/** @file
 * Output bus messages via printf.
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
 * A HostSink receives data over a serial bus, buffers them and outputs
 * the buffer later via printf.
 *
 * State: stable
 * Features: printf output, buffering, overflow indication
 */
class HostSink : public StaticReceiver<HostSink>
{
  unsigned _hdev;
  unsigned _size;
  unsigned _count;
  bool  _overflow;
  unsigned _head_char;
  unsigned _cont_char;
  unsigned char *_buffer;

 public:
  bool  receive(MessageSerial &msg)
  {
    if (msg.serial != _hdev)   return false;
    if (msg.ch == '\r')
      return true;
    if (msg.ch == '\n' || _count == _size -1)
      {
	_buffer[_count] = 0;
	if (_overflow)
	  Logging::printf("%c %c   %s\n", _head_char, _cont_char, _buffer);
	else
	  Logging::printf("%c   %s\n", _head_char, _buffer);
	_overflow = _count == _size -1;
	_count = 0;
      }
    if (msg.ch != '\n')
      _buffer[_count++] = msg.ch;
    return true;
  }

 HostSink(unsigned hdev, unsigned size, unsigned head_char, unsigned cont_char) : _hdev(hdev), _size(size), _count(0), _overflow(false)
  {
    if ((size == ~0UL) || (size < 1))
      size = 1;
    _head_char = (head_char == ~0UL) ? '#' : head_char;
    _cont_char = (cont_char == ~0UL) ? '|' : cont_char;
    _buffer = new unsigned char[size];
  }
};

PARAM_HANDLER(hostsink,
	      "hostsink:hostdevnr,bufferlen,sinkchar,contchar - provide an output for a serial port.",
	      "Example: 'hostsink:0x4712,80'.")
{
  mb.bus_serial.add(new HostSink(argv[0], argv[1], argv[2], argv[3]), HostSink::receive_static<MessageSerial>);
}
