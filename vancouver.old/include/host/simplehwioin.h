/**
 * SimpleHWIOIn template.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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

DBus<MessageIOIn>   &_bus_hwioin;

unsigned char inb(unsigned short port)
{
  MessageIOIn msg(MessageIOIn::TYPE_INB, port);
  if (!_bus_hwioin.send(msg)) 
    Logging::panic("%s could not read from ioport %x\n", __PRETTY_FUNCTION__, port);
  return msg.value;
}
