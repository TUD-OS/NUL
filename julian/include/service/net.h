/** -*- Mode: C++ -*-
 * IPv4/TCP/UDP checksum calculation
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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
#include <service/math.h>


class IPChecksum {
public:
  
  /// Sum a vector of 16-bit big endian values. If len is odd, the
  /// last byte is padded with a 0 byte from the end.
  static uint32 comp16sum(uint8 *buf, unsigned len,
                          uint32 initial)
  {
    uint32 sum = initial;
    for (unsigned i = 0; i < (len&~1); i += 2)
      sum += Math::htons(*reinterpret_cast<uint16*>(buf+i));
    if ((len&1) != 0)
      // XXX Endianess
      sum += buf[len-1]<<8;
    return sum;
  }

  /// Correct the result of comp16sum. The result is suitable to be
  /// used as IP/TCP/UDP checksum.
  static uint16 comp16correct(uint32 v)
  {
    return Math::htons(~(v + (v>>16)));
  }

  /// Compute an IP checksum.
  static uint16 ipsum(uint8 *buf, unsigned maclen, unsigned iplen)
  {
    return comp16correct(comp16sum(buf + maclen, iplen, 0));
  }

  /// Compute TCP/UDP checksum. proto is 17 for UDP and 6 for TCP.
  static uint16 tcpudpsum(uint8 *buf, uint8 proto,
                          unsigned maclen, unsigned iplen,
                          unsigned len)
  {
    uint32 sum = 0;
  
    sum = comp16sum(buf + maclen + iplen, len - maclen - iplen, sum);
    sum = comp16sum(buf + maclen + 12, 8, sum);

    uint16 p[] = { static_cast<uint16>(proto << 8), Math::htons(len - maclen - iplen) };
    sum = comp16sum(reinterpret_cast<uint8 *>(p), sizeof(p), sum);

    return comp16correct(sum);
  }
};

// EOF
