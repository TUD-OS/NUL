/** @file
 * SimpleHWIOIn template.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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

DBus<MessageHwIOIn>   &_bus_hwioin;

unsigned char inb(unsigned short port)
{
  MessageHwIOIn msg(MessageIOIn::TYPE_INB, port);
  if (!_bus_hwioin.send(msg, true))
    Logging::panic("%s could not read from ioport %x\n", __PRETTY_FUNCTION__, port);
  return msg.value;
}

void insw(void *ptr, unsigned count, unsigned short port)
{
  MessageHwIOIn msg(MessageIOIn::TYPE_INW, port, count, ptr);
  if (!_bus_hwioin.send(msg, true))
    Logging::panic("%s could not read from ioport %x\n", __PRETTY_FUNCTION__, port);
}

void insl(void *ptr, unsigned count, unsigned short port)
{
  MessageHwIOIn msg(MessageIOIn::TYPE_INL, port, count, ptr);
  if (!_bus_hwioin.send(msg, true))
    Logging::panic("%s could not read from ioport %x\n", __PRETTY_FUNCTION__, port);
}
