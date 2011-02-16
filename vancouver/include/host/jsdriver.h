/** -*- Mode: C++ -*-
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

#pragma once

#include <nul/types.h>
#include <service/logging.h>
#include <nul/motherboard.h>

class PciDriver {
protected:
  // Misc
  Clock *_clock;

  void spin(unsigned micros);
  bool wait(volatile uint32 &reg, uint32 mask, uint32 value,
	    unsigned timeout_micros = 1000000 /* 1s */);

  /// Logging
  unsigned _msg_level;
  uint16 _bdf;

  // Messages are tagged with one or more constants from this
  // bitfield. You can disable certain kinds of messages in the
  // constructor.
  enum MessageLevel {
    INFO  = 1<<0,
    DEBUG = 1<<1,
    PCI   = 1<<2,
    IRQ   = 1<<3,
    RX    = 1<<4,
    TX    = 1<<5,
    VF    = 1<<6,

    ALL   = ~0U,
  };

  __attribute__ ((format (printf, 3, 4))) void msg(unsigned level, const char *msg, ...);

  PciDriver(Clock *clock, unsigned msg_level, uint16 bdf)
    : _clock(clock), _msg_level(msg_level), _bdf(bdf)
  {}
};

// EOF
