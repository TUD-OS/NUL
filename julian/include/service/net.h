/**
 * IPv4/TCP/UDP checksum calculation
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
#include <service/endian.h>


class IPChecksum {
protected:

  static inline void
  comp16sum_simple(uint8 &lo, uint32 &state, const uint8 *buf, uint16 buf_len)
  {
    for (uint16 i = 0; i < buf_len; i++)
      state += (lo ^= 1) ? (buf[i]<<8) : buf[i];
  }


  static inline void
  comp16sum_fast(uint8 &lo, uint32 &state, const uint8 *buf, uint16 buf_len)
  {
    // // Align buf pointer
    // uint16 alen = 4 - reinterpret_cast<uintptr_t>(buf)&3;
    // process(buf, alen);
    // buf     += alen;
    // buf_len -= alen;

    // Main loop
    const uint32 *buf4 = reinterpret_cast<const uint32 *>(buf);
    const uint16  words = buf_len>>2;
    if (lo)
      for (uint16 i = 0; i < words; i++) {
        uint32    c = buf4[i];
        state += (c & 0xFFFF) + ((c >> 16) & 0xFFFF);
      }
    else
      for (uint16 i = 0; i < words; i++) {
        uint32    c = buf4[i];
        c = (c << 24) | (c >> 8);
        state += (c & 0xFFFF) + ((c >> 16) & 0xFFFF);
      }

    // Handle unaligned rest
    comp16sum_simple(lo, state, buf + (buf_len&~3), buf_len & 3);
  }

public:
  
  /// Correct the result of comp16sum. The result is suitable to be
  /// used as IP/TCP/UDP checksum.
  static uint16 comp16correct(uint32 v)
  {
    return Endian::hton16(~(v + (v>>16)));
  }

  /// Compute an IP checksum.
  static uint16 ipsum(const uint8 *buf, unsigned maclen, unsigned iplen)
  {
    uint8  lo    = 0;
    uint32 state = 0;
    comp16sum_fast(lo, state, buf + maclen, iplen);
    return comp16correct(state);
  }

  /// Compute TCP/UDP checksum. proto is 17 for UDP and 6 for TCP.
  static uint16
  tcpudpsum(const uint8 *buf, uint8 proto,
	    unsigned maclen, unsigned iplen,
	    unsigned len)
  {
    uint8  lo  = 0;
    uint32 sum = 0;

    // Source and destination IP addresses (part of pseudo header)
    comp16sum_fast(lo, sum, buf + maclen + 12, 8);

    // Second part of pseudo header: 0, protocol ID, UDP length
    uint16 p[] = { static_cast<uint16>(proto << 8), Endian::hton16(len - maclen - iplen) };
    comp16sum_fast(lo, sum, reinterpret_cast<uint8 *>(p), sizeof(p));

    // Sum L4 header plus payload
    comp16sum_fast(lo, sum, buf + maclen + iplen, len - maclen - iplen);

    return comp16correct(sum);
  }
};

// EOF
