/**
 * SimpleMemoryAccess template.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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

DBus<MessageMemAlloc>   *_bus_memalloc;
DBus<MessageMem>        *_bus_mem;

/**
 * copy from buffer to the guest.
 *
 * Missing: optimize via MemAlloc
 */
bool copy_out(unsigned long address, void *ptr, unsigned count)
{
  char *p = reinterpret_cast<char *>(ptr);
  if (address & 3) {
    unsigned value;
    MessageMem msg(true, address & ~3, &value);
    if (!_bus_mem->send(msg)) return false;
    unsigned l = 4 - (address & 3);
    if (l > count) l = count;
    memcpy(reinterpret_cast<char *>(&value) + (address & 3), p, l);
    msg.read = false;
    if (!_bus_mem->send(msg)) return false;
    p       += l;
    address += l;
    count   -= l;
  }
  assert(!(address & 3));
  while (count >= 4) {
    MessageMem msg(false, address, reinterpret_cast<unsigned *>(p));
    if (!_bus_mem->send(msg)) return false;
    address += 4;
    p       += 4;
    count   -= 4;
  }
  if (count) {
    unsigned value;
    MessageMem msg(true, address, &value);
    if (!_bus_mem->send(msg)) return false;
    memcpy(&value, p, count);
    msg.read = false;
    if (!_bus_mem->send(msg)) return false;
  }
  return true;
}


/**
 * copy from the guest to a buffer.
 *
 * Missing: optimize via MemAlloc
 */
bool copy_in(unsigned long address, void *ptr, unsigned count)
{
  char *p = reinterpret_cast<char *>(ptr);
  if (address & 3) {
    unsigned value;
    MessageMem msg(true, address & ~3, &value);
    if (!_bus_mem->send(msg)) return false;
    value >>= 8*(address & 3);
    unsigned l = 4 - (address & 3);
    if (l > count) l = count;
    memcpy(p, &value,  l);
    p       += l;
    address += l;
    count   -= l;
  }
  assert(!(address & 3));
  while (count >= 4) {
    MessageMem msg(true, address, reinterpret_cast<unsigned *>(p));
    if (!_bus_mem->send(msg)) return false;
    address += 4;
    p       += 4;
    count   -= 4;
  }
  if (count) {
    unsigned value;
    MessageMem msg(true, address, &value);
    if (!_bus_mem->send(msg)) return false;
    memcpy(p, &value, count);
  }
  return true;
}
