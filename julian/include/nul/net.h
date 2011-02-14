// -*- Mode: C++ -*-
/*
 * Intel 82576 DMA descriptors.
 *
 * Copyright (C) 2010-2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <nul/types.h>
#include <nul/compiler.h>

typedef struct {
  enum {
    DTYP_CONTEXT = 2U,
    DTYP_DATA    = 3U,

    DCMD_TSE     = (1U<<7),

    POPTS_IXSM   = 1U,          // Insert IP checksum
    POPTS_TXSM   = 2U,          // Insert L4 checksum
    POPTS_IPSEC  = 4U,          // IPsec offloading
    
    L4T_UDP  = 0U,
    L4T_TCP  = 1U,
    L4T_SCTP = 2U,
  };

  union {
    uint64 raw[2];
    uint32 rawd[4];
  };

  bool  is_done()       { return (rawd[3] & 1); }
  void  set_done()      { raw[1] = (1ULL << 32 /* DD */); }
  bool  rs()      const { return (rawd[2] & (1 << 27 /* RS */)); }
  uint8 idx()     const { return (rawd[3] >> 4) & 0x7; } 
  uint8 dtyp()    const { return (rawd[2] >> 20) & 0xF; }
  uint8 dcmd()    const { return (rawd[2] >> 24) & 0xFF; }
  uint8 popts()   const { return (rawd[3] >> 8) & 0x3F; }
  bool  legacy()  const { return (rawd[2] & (1<<29 /* DEXT */)) == 0; }
  uint16 paylen() const { return (rawd[3] >> 14); }
  uint16 dtalen() const { return (rawd[2] & 0xFFFF); }
  bool  eop() const
  {
    if (legacy())
      return (raw[1] & (1<<24 /* EOP */)) != 0;
    else
      return (dtyp() == DTYP_CONTEXT) || ((raw[1] & (1<<24 /* EOP */)) != 0);
  }

  // Context
  uint16 tucmd()  const { return (rawd[2] >> 9) & 0x3FF; }
  uint8  l4t()    const { return (rawd[2] >> (9+2)) & 0x3; }
  uint8  l4len()  const { return (rawd[3] >> 8) & 0xFF; }
  uint16 mss()    const { return (rawd[3] >> 16) & 0xFFFF; }
  uint16 iplen()  const { return rawd[0] & 0x1FF; }
  uint8  maclen() const { return (rawd[0] >> 9) & 0x7F; }

} tx_desc;

typedef struct {

  union {
    uint64 raw[2];
    uint32 rawd[4];
  };

  void set_done(uint8 type, uint16 len, bool eop)
  {
    switch (type) {
    case 0:			// Legacy
      raw[1] = ((eop ? 0x3ULL /* EOP, DD */ : 0x1ULL /* DD */) << 32) | len;
      break;
    case 1:			// Advanced, one buffer
      raw[0] = 0;
      raw[1] = static_cast<uint64>(len)<<32 | 
	(eop ? 0x3 /* EOP, DD */ : 0x1 /* DD */);
      break;
    default:
#ifndef BENCHMARK
      Logging::printf("Invalid descriptor type %x\n", type);
#endif
      break;
    }
  }
} rx_desc;

/* EOF */
