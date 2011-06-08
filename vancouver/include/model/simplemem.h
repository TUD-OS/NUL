/** @file
 * SimpleMemoryAccess template.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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

DBus<MessageMemRegion>  *_bus_memregion;
DBus<MessageMem>        *_bus_mem;


/**
 * Fast copy/inout version that only copies to mem-regions but not registers.
 */
bool copy_inout(unsigned long address, void *ptr, unsigned count, bool read)
{
  MessageMemRegion msg(address >> 12);
  if (!_bus_memregion->send(msg) || !msg.ptr || ((address + count) > ((msg.start_page + msg.count) << 12))) return false;
  if (read)
    memcpy(ptr, msg.ptr + (address - (msg.start_page << 12)), count);
  else
    memcpy(msg.ptr + (address - (msg.start_page << 12)), ptr, count);
  return true;
}

/**
 * Copy from buffer to the guest.
 */
bool copy_out(unsigned long address, void *ptr, unsigned count)
{
  if (copy_inout(address, ptr, count, false)) return true;
  char *p = reinterpret_cast<char *>(ptr);
  if (address & 3) {
    unsigned value;
    MessageMem msg(true, address & ~3, &value);
    if (!_bus_mem->send(msg, true)) return false;
    unsigned l = 4 - (address & 3);
    if (l > count) l = count;
    memcpy(reinterpret_cast<char *>(&value) + (address & 3), p, l);
    msg.read = false;
    if (!_bus_mem->send(msg, true)) return false;
    p       += l;
    address += l;
    count   -= l;
  }
  assert(!(address & 3));
  while (count >= 4) {
    MessageMem msg(false, address, reinterpret_cast<unsigned *>(p));
    if (!_bus_mem->send(msg, true)) return false;
    address += 4;
    p       += 4;
    count   -= 4;
  }
  if (count) {
    unsigned value;
    MessageMem msg(true, address, &value);
    if (!_bus_mem->send(msg, true)) return false;
    memcpy(&value, p, count);
    msg.read = false;
    if (!_bus_mem->send(msg, true)) return false;
  }
  return true;
}


/**
 * Copy from the guest to a buffer.
 */
bool copy_in(unsigned long address, void *ptr, unsigned count)
{
  if (copy_inout(address, ptr, count, true)) return true;
  char *p = reinterpret_cast<char *>(ptr);
  if (address & 3) {
    unsigned value;
    MessageMem msg(true, address & ~3, &value);
    if (!_bus_mem->send(msg, true)) return false;
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
    if (!_bus_mem->send(msg, true)) return false;
    address += 4;
    p       += 4;
    count   -= 4;
  }
  if (count) {
    unsigned value;
    MessageMem msg(true, address, &value);
    if (!_bus_mem->send(msg, true)) return false;
    memcpy(p, &value, count);
  }
  return true;
}
