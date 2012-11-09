// -*- Mode: C++ -*-
/** @file
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

#include <service/hexdump.h>

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
  { byte[0] = b1; byte[1] = b2; byte[2] = b3; byte[3] = b4; byte[4] = b5; byte[5] = b6; }
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

  static inline uint32 
  sum_simple(uint8 const *buf, size_t size, bool &odd)
  {
    // Cannot sum more because of overflow
    assert(size < 65535);

    uint32 astate = 0;

    //Logging::printf("sum_simple %u %u\n", size, odd);
    if (odd and (size != 0)) {
      astate += *(buf++);
      size--;
      odd = false;
      //Logging::printf("corrected initial oddness: %u %u\n", size, odd);
    }

    while (size >= 2) {
      uint16 const *buf16 = reinterpret_cast<uint16 const *>(buf);
      astate += *buf16;
      buf  += 2;
      size -= 2;
    }

    if (size != 0) {
      // XXX Correct?
      astate += *buf;
      odd = true;
      //Logging::printf("Odd bytes. beware...\n");
    } else
      odd = false;

    //Logging::printf("<- %u %u\n", size, odd);
    return astate;
  }
  
  // Sums data until buf is 16-byte aligned. This handles odd numbers
  // of align_steps. You should continue in odd mode, if this happens!
  static inline uint32
  sum_align(uint8 const * &buf, size_t &size, unsigned &align_steps, bool &odd)
  {
    //Logging::printf("align %u %u\n", align_steps, odd);
    if (align_steps == 0) return 0;
    //assert((align_steps % 2) == 0);
    if (align_steps > size)
      align_steps = size;

    uint32 res = sum_simple(buf, align_steps, odd);

    buf  += align_steps;
    size -= align_steps;
    return res;
  }

  static inline  __attribute__((always_inline)) uint32
  addoc(uint32 a, uint32 b)
  {
    asm ("add %1, %0;"
         "adc $0, %0;" : "+r" (a) : "rm" (b));
    return a;
  }

#ifdef __SSE2__
  static inline __attribute__((always_inline))  __m128i
  sse_step(__m128i sum, __m128i v1, __m128i v2, __m128i z)
  {
    sum = _mm_add_epi64(sum, _mm_add_epi64(_mm_unpackhi_epi32(v1, z),
                                           _mm_unpacklo_epi32(v1, z)));;
    
    sum = _mm_add_epi64(sum, _mm_add_epi64(_mm_unpackhi_epi32(v2, z),
                                           _mm_unpacklo_epi32(v2, z)));
    return sum;
  }

  static inline  __attribute__((always_inline)) uint32
  sse_final(__m128i sum, __m128i z, bool odd)
  {
    /* Add top to bottom 64-bit word */
    sum = _mm_add_epi64(sum, _mm_srli_si128(sum, 8));
    
    /* Add low two 32-bit words */
#ifdef __SSSE3__
    sum = _mm_hadd_epi32(sum, z);
#else  // No SSSE3
#warning SSSE3 not available
    sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));
#endif
    if (odd) {
      // XXX
      Logging::printf("sse_final ODD %08x -> %04x -> %04x\n", _mm_cvtsi128_si32(sum),
                      fixup(_mm_cvtsi128_si32(sum)), Endian::hton16(fixup(_mm_cvtsi128_si32(sum))));
      return Endian::hton16(fixup(_mm_cvtsi128_si32(sum)));
    } else
      return _mm_cvtsi128_si32(sum);
  }
#endif

public:

  // Compute the final 16-bit checksum from our internal checksum
  // state.
  static uint16 fixup(uint32 state)
  {
    // Add all carry values.
    uint32 v = (state & 0xFFFF) + (state >> 16);
    return (v + (v >> 16));
  }

  // Update a checksum state
  static void
  sum(uint8 const *buf, size_t size, uint32 &state, bool &odd)
  {
    //Logging::printf("sum(%p, %u, %08x, %u)\n", buf, size, state, odd);
    //hexdump(buf, size);
    uint8 const *buf_end = buf + size;
    unsigned cstate      = state;

    // Step 1: Align buffer to 16 byte (for SSE)
    unsigned align_steps = 0xF & (0x10 - (reinterpret_cast<mword>(buf) & 0xF));
    cstate = addoc(cstate, sum_align(buf, size, align_steps, odd));
    //Logging::printf("sum %u align steps\n", align_steps);

    // Step 2: Checksum in large, aligned chunks
#ifdef __SSE2__
    {
      const __m128i z = _mm_setzero_si128();
      __m128i     sum = _mm_setzero_si128();

      while (size > 32) {
        assert((buf + 32) <= buf_end);

        __m128i v1 = _mm_load_si128(reinterpret_cast<__m128i const *>(buf));
        __m128i v2 = _mm_load_si128(reinterpret_cast<__m128i const *>(buf) + 1);

        sum = sse_step(sum, v1, v2, z);

        size -= 32;
        buf  += 32;
      }
      
      cstate = addoc(cstate, sse_final(sum, z, odd));
    }
#endif

    // Step 3: Checksum unaligned rest
    uint32 rstate = sum_simple(buf, size, odd);
    cstate = addoc(cstate, rstate);

    state = cstate;
    //Logging::printf("sum() -> %08x %04x %u\n", state, fixup(state), odd);
  }
  
  /// Compute an IP checksum.
  static uint16 ipsum(const uint8 *buf, unsigned maclen, unsigned iplen)
  {
    uint32 state = 0;
    bool   odd   = false;
    sum(buf + maclen, iplen, state, odd);
    return ~fixup(state);
  }

  /// Compute TCP/UDP checksum. proto is 17 for UDP and 6 for TCP.
  static uint16
  tcpudpsum(const uint8 *buf, uint8 proto,
	    unsigned maclen, unsigned iplen,
	    unsigned len, bool ipv6 = false)
  {
    //Logging::printf("--- tcpudpsum(%p) \n", buf);
    uint32 state = 0;
    bool   odd   = false;

    if (not ipv6) {
      // IPv4:
      // Source and destination IP addresses (part of pseudo header)
      sum(buf + maclen + 12, 8, state, odd);
      
      // Second part of pseudo header: 0, protocol ID, UDP length
      const uint16 p[] = { static_cast<uint16>(proto << 8), Endian::hton16(len - maclen - iplen) };
      sum(reinterpret_cast<const uint8 *>(p), sizeof(p), state, odd);
    } else {
      // IPv6:
      // Source and destination IP addresses (part of pseudo header)
      sum(buf + maclen + 8, 2*16, state, odd);
      const uint32 pseudo2[2] = {Endian::hton32(len - maclen - iplen),
                                 Endian::hton32(proto) };
      sum(reinterpret_cast<const uint8 *>(pseudo2), sizeof(pseudo2), state, odd);
    }
      
    // Sum L4 header plus payload
    sum(buf + maclen + iplen, len - maclen - iplen, state, odd);
    return ~fixup(state);
  }

  // Move data and update TCP/IP checksum.
  static void
  move(uint8 * dst, uint8 const * src, unsigned size, uint32 &state, bool &odd)
  {
    // Logging::printf("move(%p, %p, %u, %08x, %u)\n", dst, src, size, state, odd);
    // hexdump(src, size);

    // Step 1: Align dst
    mword dsti = reinterpret_cast<mword>(dst);

    {
      unsigned align_steps = 0xF & (0x10 - (dsti & 0xF));
      //Logging::printf("move %u align steps odd %u\n", align_steps, odd);

      state = addoc(state, sum_align(src, size, align_steps, odd));
      memcpy(dst, src - align_steps, align_steps);

      // src is already set by sum_align
      dst += align_steps;

      //assert(memcmp(dst-align_steps, src-align_steps, align_steps) == 0);
      //Logging::printf("move aligned %u\n", odd);
    }

#ifdef __SSE2__
    // Step 2: The Heavy Lifting
    {
      const __m128i z = _mm_setzero_si128();
      __m128i     sum = _mm_setzero_si128();
    
      while (size > 32) {
        __m128i v1 = _mm_loadu_si128(reinterpret_cast<__m128i const *>(src));
        __m128i v2 = _mm_loadu_si128(reinterpret_cast<__m128i const *>(src) + 1);
      
        sum = sse_step(sum, v1, v2, z);

        _mm_stream_si128(reinterpret_cast<__m128i *>(dst), v1);
        _mm_stream_si128(reinterpret_cast<__m128i *>(dst) + 1, v2);
      
        size -= 32;
        src  += 32;
        dst  += 32;
      }

      state = addoc(state, sse_final(sum, z, odd));
    }
#else
#warning IPChecksum::move built without SSE. This is terribly slow.
#endif

    // Step 3: The Trail
    //if (size == 0) return;

    state = addoc(state, sum_simple(src, size, odd));
    memcpy(dst, src, size);

    //assert(memcmp(dst, src, size) == 0);

    //Logging::printf("move() -> %08x %04x %u\n", state, fixup(state), odd);
  }

};

class IPChecksumState : protected IPChecksum {
private:
  uint32 _state;
  bool   _odd;

public:

  /// Update checksum state with TCP/UDP pseudo header. proto is 17
  /// for UDP and 6 for TCP.
  void
  update_l4_header(uint8 const * buf,
                   uint8 proto,
                   unsigned maclen, unsigned iplen,
                   unsigned len)
  {
    assert(_state == 0);
    assert(not _odd);

    //Logging::printf("--- update_l4_header\n");

    // Source and destination IP addresses (part of pseudo header)
    sum(buf + maclen + 12, 8, _state, _odd);

    // Second part of pseudo header: 0, protocol ID, UDP length
    uint16 p[] = { static_cast<uint16>(proto << 8), Endian::hton16(len - maclen - iplen) };
    sum(reinterpret_cast<uint8 *>(p), sizeof(p), _state, _odd);

    //Logging::printf("update_l4_header() -> %08x %u\n", _state, _odd);
    assert(not _odd);
  }

  void update(uint8 const * buf, size_t len)
  {
    //Logging::printf("update(%p, %u) %08x %u\n", buf, len, _state, _odd);
    IPChecksum::sum(buf, len, _state, _odd);
    //Logging::printf("update() -> %08x %u\n", _state, _odd);
  }

  void
  move(uint8 * dst, uint8 const * src, unsigned len)
  {
    //Logging::printf("move(%p, %p, %u) %08x %u\n", dst, src, len, _state, _odd);
    IPChecksum::move(dst, src, len, _state, _odd);
    //Logging::printf("move() -> %08x %u\n", _state, _odd);
    //assert(memcmp(dst, src, len) == 0);
  }

  uint16 value() const {
    //Logging::printf("value() -> %08x %04x %04x\n", _state, fixup(_state), ~fixup(_state));
    return ~fixup(_state);
  }

  IPChecksumState() : _state(0), _odd(false) {}
};

// EOF
