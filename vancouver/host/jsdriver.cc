/**
 * Common driver routines.
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include <host/jsdriver.h>

void
PciDriver::spin(unsigned micros)
{
  timevalue done = _clock->abstime(micros, 1000000);
  while (_clock->time() < done)
    Cpu::pause();
}

bool
PciDriver::wait(volatile uint32 &reg, uint32 mask, uint32 value,
		unsigned timeout_micros)
{
  timevalue timeout = _clock->abstime(timeout_micros, 1000000);
  
  while ((reg & mask) != value) {
    Cpu::pause();
    if (_clock->time() >= timeout)
      return false;
  }
  return true;
}

void
PciDriver::msg(unsigned level, const char *msg, ...)
{
  if ((level & _msg_level) != 0) {
    va_list ap;
    va_start(ap, msg);
    Logging::printf("82576 %02x: ", _bdf & 0xFF);
    Logging::vprintf(msg, ap);
    va_end(ap);
  }
}

// EOF
