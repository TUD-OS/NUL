/**
 * SimpleDiscovery template.
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

DBus<MessageDiscovery> &_bus_discovery;

/**
 * Write a string to a resource.
 */
bool discovery_write_st(const char *resource, unsigned offset, const void *value, unsigned count) {
  MessageDiscovery msg(resource, offset, value, count);
  return _bus_discovery.send(msg);
}


/**
 * Write a dword or less than it.
 */
bool discovery_write_dw(const char *resource, unsigned offset, unsigned value, unsigned count = 4)  {
  return discovery_write_st(resource, offset, &value, count);
}


/**
 * Read a dword.
 */
bool discovery_read_dw(const char *resource, unsigned offset, unsigned &value)  {
  MessageDiscovery msg(resource, offset, &value);
  return _bus_discovery.send(msg);
}

