/** @file
 * Direct IOIO access.
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

#include "nul/motherboard.h"

/**
 * Provide IOIO access to the host.
 *
 * State: stable
 * Features: in, out, ins, outs
 */
class IOAccess : public StaticReceiver<IOAccess>
{
public:
  /**
   * Access the hardware.
   */
  bool  receive(MessageIOIn &msg)
  {
    if (!msg.count)
      {
	switch (msg.type)
	  {
	  case MessageIOIn::TYPE_INB: asm volatile("inb %1, %b0" : "=a"(msg.value): "Nd"(msg.port)); break;
	  case MessageIOIn::TYPE_INW: asm volatile("inw %1, %w0" : "=a"(msg.value): "Nd"(msg.port)); break;
	  case MessageIOIn::TYPE_INL: asm volatile("inl %1, %0" : "=a"(msg.value): "Nd"(msg.port)); break;
	  default:
	    Logging::panic("%s:%d wrong type %x", __PRETTY_FUNCTION__, __LINE__, msg.type);
	  };
      }
    else
      {
	switch (msg.type)
	  {
	  case MessageIOIn::TYPE_INB: asm volatile("rep insb" : "+D"(msg.ptr), "+c"(msg.count) : "d"(msg.port) : "memory"); break;
	  case MessageIOIn::TYPE_INW: asm volatile("rep insw" : "+D"(msg.ptr), "+c"(msg.count) : "d"(msg.port) : "memory"); break;
	  case MessageIOIn::TYPE_INL: asm volatile("rep insl" : "+D"(msg.ptr), "+c"(msg.count) : "d"(msg.port) : "memory"); break;
	  default:
	    Logging::panic("%s:%d wrong type %x", __PRETTY_FUNCTION__, __LINE__, msg.type);
	  };
      }
    return true;
  }


  /**
   * Access the hardware.
   */
  bool  receive(MessageIOOut &msg)
  {
    if (!msg.count)
      {
	switch (msg.type)
	  {
	  case MessageIOOut::TYPE_OUTB: asm volatile("outb %b0,%1" :: "a"(msg.value),"Nd"(msg.port)); break;
	  case MessageIOOut::TYPE_OUTW: asm volatile("outw %w0,%1" :: "a"(msg.value),"Nd"(msg.port)); break;
	  case MessageIOOut::TYPE_OUTL: asm volatile("outl %0,%1" :: "a"(msg.value),"Nd"(msg.port)); break;
	  default:
	    Logging::panic("%s wrong type %x", __PRETTY_FUNCTION__, msg.type);
	  };
      }
    else
      {
	switch (msg.type)
	  {
	  case MessageIOOut::TYPE_OUTB: asm volatile("rep outsb" : "+S"(msg.ptr), "+c"(msg.count) : "d"(msg.port) : "memory"); break;
	  case MessageIOOut::TYPE_OUTW: asm volatile("rep outsw" : "+S"(msg.ptr), "+c"(msg.count) : "d"(msg.port) : "memory"); break;
	  case MessageIOOut::TYPE_OUTL: asm volatile("rep outsl" : "+S"(msg.ptr), "+c"(msg.count) : "d"(msg.port) : "memory"); break;
	  default:
	    Logging::panic("%s wrong type %x", __PRETTY_FUNCTION__, msg.type);
	  };
      }
    return true;
  }
};


PARAM_HANDLER(ioio,
	      "ioio - provide HW IO port access.")
{
  IOAccess *dev = new IOAccess();
  mb.bus_hwioin.add(dev,  IOAccess::receive_static<MessageHwIOIn>);
  mb.bus_hwioout.add(dev, IOAccess::receive_static<MessageHwIOOut>);
}
