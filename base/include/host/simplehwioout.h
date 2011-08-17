/** @file
 * SimpleHWIOout template.
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

DBus<MessageHwIOOut>  &_bus_hwioout;

void outb(unsigned char value, unsigned short port)
{
  MessageHwIOOut msg(MessageIOOut::TYPE_OUTB, port, value);
  if (!_bus_hwioout.send(msg, true))
    Logging::panic("%s could not send to ioport %x\n",  __PRETTY_FUNCTION__, port);
}

void outw(unsigned short value, unsigned short port)
{
  MessageHwIOOut msg(MessageIOOut::TYPE_OUTW, port, value);
  if (!_bus_hwioout.send(msg, true))
    Logging::panic("%s could not send to ioport %x\n",  __PRETTY_FUNCTION__, port);
}

void outsw(void *ptr, unsigned count, unsigned short port)
{
  MessageHwIOOut msg(MessageIOOut::TYPE_OUTW, port, count, ptr);
  if (!_bus_hwioout.send(msg, true))
    Logging::panic("%s could not send to ioport %x\n", __PRETTY_FUNCTION__, port);
}

void outsl(void *ptr, unsigned count, unsigned short port)
{
  MessageHwIOOut msg(MessageIOOut::TYPE_OUTL, port, count, ptr);
  if (!_bus_hwioout.send(msg, true))
    Logging::panic("%s could not send to ioport %x\n", __PRETTY_FUNCTION__, port);
}
