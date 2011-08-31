/*
 * Client part of the bridge protocol.
 *
 * Copyright (C) 2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
#pragma once

#include <nul/error.h>
#include <nul/parent.h>

template <unsigned SIZE>
struct PacketRing {
  volatile unsigned _rpos;
  volatile unsigned _wpos;
  
  static_assert(SIZE % sizeof(unsigned) == 0, "SIZE needs to be aligned");
  static const unsigned ELEMENTS = SIZE/sizeof(unsigned) - 2;
  unsigned    _buffer[ELEMENTS];

  bool has_data() const { return _rpos != _wpos; }

  bool produce(const uint8 *buf, size_t len)
  {
    assert(len != 0);

    unsigned wpos   = _wpos;
    unsigned rpos   = _rpos;
    unsigned right  = sizeof(_buffer) - wpos;
    unsigned left   = rpos;
    unsigned needed = (len + 2*sizeof(unsigned) - 1) / sizeof(unsigned);

    if (left > _wpos) {
      right = left - wpos;
      left  = 0;
    }

    if ((needed > right) && (needed > left))
      // Dropped
      return false;

    unsigned ofs = wpos;
    if (right < needed) {
      if (right != 0) _buffer[ofs] = ~0U;
      ofs = 0;
    }

    _buffer[ofs] = len;
    memcpy(_buffer + ofs + 1, buf, len);
    assert((ofs + needed)*sizeof(unsigned) <= sizeof(_buffer));
    if (ofs + needed == sizeof(_buffer)/sizeof(unsigned))
      _wpos = 0;
    else
      _wpos = ofs + needed;
    MEMORY_BARRIER;
    return true;
  }

  unsigned get_buffer(uint8 *&buffer)
  {
    unsigned *plen = _buffer + _rpos;

    if (*plen == ~0U) {
      _rpos = 0;
      plen = _buffer + 0;
    }
    
    unsigned len = *plen;
    assert(len < (sizeof(_buffer) - sizeof(unsigned)));
    buffer = reinterpret_cast<uint8 *>(_buffer + _rpos + 1);
    return len;
  }

  void free_buffer()
  {
    _rpos = (_rpos + _buffer[_rpos] + 2*sizeof(unsigned) - 1)/sizeof(unsigned) % ELEMENTS;
  }


  static PacketRing<SIZE> *from_void(void *x) { return reinterpret_cast<PacketRing<SIZE> *>(x); }
};

static_assert(sizeof(PacketRing<4096>) == 4096, "Our alignment assumptions don't hold.");

struct BridgeProtocol : public GenericProtocol {

  enum {
    RING_BUFFER_SHIFT = 14,
    RING_BUFFER_SIZE = (1U << RING_BUFFER_SHIFT),

    MEMORY_SHIFT     = 1 + RING_BUFFER_SHIFT,
    MEMORY_SIZE      = 2*RING_BUFFER_SIZE,
  };

  enum {
    TYPE_GET_RING    = ParentProtocol::TYPE_GENERIC_END,
    TYPE_GET_TX_SEMAPHORE,
    TYPE_GET_RX_SEMAPHORE,
    TYPE_SEND_PACKET,
  };

private:
  char                             *_rings;
  unsigned                          _tx_sm;
  unsigned                          _rx_sm;

  PacketRing<RING_BUFFER_SIZE> *_tx_ring;
  PacketRing<RING_BUFFER_SIZE> *_rx_ring;
  
  void request_resource(unsigned id, Crd d)
  {
    unsigned res = call_server(init_frame(*BaseProgram::myutcb(), id) << d, true);
    if (res != ENONE) Logging::panic("Could not establish shared memory.");
  }

  unsigned ensure_rings()
  {
    if (_tx_ring == NULL) {
      assert( _rings != NULL );
      assert((reinterpret_cast<unsigned long>(_rings) & (MEMORY_SIZE - 1)) == 0);

      // OMG. This sucks... But the current client/server IPC
      // framework makes this needlessly difficult. No, I am not going
      // to fix it today.
      request_resource(TYPE_GET_RING,         Crd(reinterpret_cast<unsigned long>(_rings), 1+RING_BUFFER_SHIFT, DESC_MEM_ALL));
      request_resource(TYPE_GET_TX_SEMAPHORE, Crd(_tx_sm, 0, DESC_CAP_ALL));
      request_resource(TYPE_GET_RX_SEMAPHORE, Crd(_rx_sm, 0, DESC_CAP_ALL));

      _tx_ring = PacketRing<RING_BUFFER_SIZE>::from_void(_rings);
      _rx_ring = PacketRing<RING_BUFFER_SIZE>::from_void(_rings + RING_BUFFER_SIZE);
    } else
      return ENONE;
  }

  public:

  /** Send a single packet. Returns false, if the packet had to be
      dropped because of missing buffer space. */
  bool send_packet(const uint8 *packet, size_t size)
  {
    ensure_rings();

    if (_tx_ring->produce(packet, size)) {
      nova_semup(_tx_sm);
      return true;
    } else
      return false;
  }

  // Returns a pointer to a received packet and its size.
  unsigned wait_packet(uint8 *&packet)
  {
    while (not _rx_ring->has_data())
      nova_semdown(_rx_sm);

    return _rx_ring->get_buffer(packet);
  }

  /// Marks the buffer returned by the last call to wait_packet as
  /// free. Must be called between wait_packet calls.
  void ack_packet() { _rx_ring->free_buffer(); }


  BridgeProtocol(unsigned cap_base,
                 unsigned sm_base, // provide 2 cap free indexes
                 char    *mem,
                 unsigned instance=0) : GenericProtocol("bridge", instance, cap_base, true)
  {
    _tx_sm = sm_base;
    _tx_sm = sm_base + 1;
    _rings = mem;
  }
};

// EOF
