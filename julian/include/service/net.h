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

enum {
  ETHERNET_ADDR_MASK = 0xFFFFFFFFFFFFULL,
};

struct EthernetAddr {
  union {
    uint64 raw;
    uint8 byte[6];
  };

  bool is_local()     const { return (byte[0] & 2) != 0; }
  bool is_multicast() const { return (byte[0] & 1) != 0; }
  bool is_broadcast() const {
    return (raw & ETHERNET_ADDR_MASK) == ETHERNET_ADDR_MASK;
  }

  EthernetAddr() : raw(0) {}
  EthernetAddr(uint8 b1, uint8 b2, uint8 b3, uint8 b4, uint8 b5, uint8 b6)
    : byte({b1, b2, b3, b4, b5, b6}) {}
  explicit EthernetAddr(uint64 _raw) : raw(_raw & ETHERNET_ADDR_MASK) {}
  
};

static inline bool
operator==(EthernetAddr const& a, EthernetAddr const& b)
{
  return ((a.raw ^ b.raw) & ETHERNET_ADDR_MASK) == 0;
}


#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_SPLIT(x) (x)->byte[0], (x)->byte[1], (x)->byte[2],(x)->byte[3], (x)->byte[4], (x)->byte[5]

class IPChecksum {
protected:

  // Compute the final 16-bit checksum from our internal checksum
  // state.
  static uint16 fixup(uint32 state)
  {
    // Add all carry values.
    uint32 v = (state & 0xFFFF) + (state >> 16);
    return (v + (v >> 16));
  }


  static uint32
  sum_simple(uint8 const *buf, size_t size)
  {
    // Cannot sum more because of overflow
    assert(size < 65535);

    uint32 astate = 0;
    while (size > 1) {
      uint16 const *buf16 = reinterpret_cast<uint16 const *>(buf);
      astate += *buf16;
      buf  += 2;
      size -= 2;
    }

    if (size != 0) {
      // XXX Correct?
      astate += *buf ;
      //Logging::printf("Odd bytes. beware...\n");
    }

    return astate;
  }
  
  // Sums data until buf is 16-byte aligned.
  static uint32
  sum_align(uint8 const *&buf, size_t &size, unsigned align_steps)
  {
    if (align_steps == 0) return 0;
    assert((align_steps % 2) == 0);
    if (align_steps > size)
      align_steps = size;

    uint32 res = sum_simple(buf, align_steps);

    buf  += align_steps;
    size -= align_steps;
    return res;
  }

  static uint32
  addoc(uint32 a, uint32 b)
  {
    asm ("add %1, %0;"
         "adc $0, %0;" : "+r" (a) : "rm" (b));
    return a;
  }

#ifdef __SSE2__
  static inline __m128i
  sse_step(__m128i sum, __m128i v1, __m128i v2, __m128i z)
  {
    sum = _mm_add_epi64(sum, _mm_add_epi64(_mm_unpackhi_epi32(v1, z),
                                           _mm_unpacklo_epi32(v1, z)));;
    
    sum = _mm_add_epi64(sum, _mm_add_epi64(_mm_unpackhi_epi32(v2, z),
                                           _mm_unpacklo_epi32(v2, z)));

    return sum;
  }
#endif

  static void
  sum(uint8 const *buf, size_t size, uint32 &state)
  {
    unsigned cstate = state;

    // Step 1: Align buffer to 16 byte (for SSE)
    unsigned align_steps = 0xF & (0x10 - (reinterpret_cast<mword>(buf) & 0xF));
    if (align_steps & 1) {
      Logging::printf("XXX Odd bytes to align: %u. do slow sum.\n", align_steps);
      // What would be the right way to support this? Is it enough to
      // bswap the result of our SSE computation if we are in "odd"
      // mode?
      goto panic_mode;
    }

    cstate = addoc(cstate, sum_align(buf, size, align_steps));

    // Step 2: Checksum in large, aligned chunks
#ifdef __SSE2__
    {
      const __m128i z = _mm_setzero_si128();
      __m128i     sum = _mm_setzero_si128();

      while (size > 32) {
        __m128i v1 = _mm_load_si128(reinterpret_cast<__m128i const *>(buf));
        __m128i v2 = _mm_load_si128(reinterpret_cast<__m128i const *>(buf) + 1);

        sum = sse_step(sum, v1, v2, z);

        size -= 32;
        buf  += 32;
      }

      /* Add top to bottom 64-bit word */
      sum = _mm_add_epi64(sum, _mm_srli_si128(sum, 8));

      /* Add low two 32-bit words */
#ifdef __SSSE3__
      sum = _mm_hadd_epi32(sum, z);
#else  // No SSSE3
#warning SSSE3 not available
      sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));
#endif
      cstate = addoc(cstate, _mm_cvtsi128_si32(sum));    
    }
#else  // No SSE
    {
      uint32 lstate = 0;
      while (size > 32) {
        uint32 const * buf32 = reinterpret_cast<uint32 const *>(buf);
        asm volatile ( "add %1, %0;"
                       "adc %2, %0;"
                       "adc %3, %0;"
                       "adc %4, %0;"
                       "adc %5, %0;"
                       "adc %6, %0;"
                       "adc %7, %0;"
                       "adc %8, %0;"
                       "adc $0, %0;"
                       : "+r" (lstate)
                       : "rm" (buf32[0]), "rm" (buf32[1]), "rm" (buf32[2]), "rm" (buf32[3]),
                         "rm" (buf32[4]), "rm" (buf32[5]), "rm" (buf32[6]), "rm" (buf32[7])
                       );

        size -= 32;
        buf  += 32;
      }
      cstate = addoc(cstate, lstate);
    }
#endif

    // Step 3: Checksum unaligned rest
  panic_mode:
    uint32 rstate = sum_simple(buf, size);
    cstate = addoc(cstate, rstate);

    state = cstate;
  }


public:
  
  /// Compute an IP checksum.
  static uint16 ipsum(const uint8 *buf, unsigned maclen, unsigned iplen)
  {
    uint32 state = 0;
    sum(buf + maclen, iplen, state);
    return ~fixup(state);
  }

  /// Compute TCP/UDP checksum. proto is 17 for UDP and 6 for TCP.
  static uint16
  tcpudpsum(const uint8 *buf, uint8 proto,
	    unsigned maclen, unsigned iplen,
	    unsigned len)
  {
    uint32 state = 0;

    // Source and destination IP addresses (part of pseudo header)
    sum(buf + maclen + 12, 8, state);

    // Second part of pseudo header: 0, protocol ID, UDP length
    uint16 p[] = { static_cast<uint16>(proto << 8), Endian::hton16(len - maclen - iplen) };
    sum(reinterpret_cast<uint8 *>(p), sizeof(p), state);

    // Sum L4 header plus payload
    sum(buf + maclen + iplen, len - maclen - iplen, state);

    return ~fixup(state);
  }
};

// EOF
