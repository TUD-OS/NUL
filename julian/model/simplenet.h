// -*- Mode: C++ -*-

#pragma once

#include <nul/net.h>
#include <host/host82576.h>	// register window layout
#include "utils.h"

class SimpleNetworkClient : private Base82576VF {
public:
  class Callback {
  public:
    virtual void send_callback(uint8 const *data) = 0;
  };

  class Memory {
  protected:
    /// Allocate page aligned memory.
    virtual void *allocate_backend(size_t bytes) = 0;
  public:
    
    template <typename T>
    T *allocate(size_t bytes) { return reinterpret_cast<T *>(allocate_backend(bytes)); }

    virtual mword ptr_to_phys(const void *ptr, size_t size) = 0;
  };

private:
  static const mword _mmio_size  = 0x4000; // 16KB
  static const mword _ring_size  = 0x1000;
  static const mword _ring_descs = _ring_size / sizeof(tx_desc);

  Memory &_mem;

  uint32 *_mmio;

  struct {
    Callback    *cb;
    uint8 const *data;
  } _tx_meta[_ring_descs];

  struct {
    uint8    *data;
  } _rx_meta[_ring_descs];

  rx_desc *_rx_ring;
  tx_desc *_tx_ring;

  unsigned _tx_to_clean;
  unsigned _rx_to_clean;

public:

  bool send_packet(uint8 const *data, size_t len, Callback *cb = NULL)
  {
    assert(len <= 0xFFFFU);

    unsigned tdt      = _mmio[TDT0];
    unsigned next_tdt = (tdt+1) % _ring_descs;
    if (next_tdt == _tx_to_clean)
      // RX queue full.
      return false;

    _tx_meta[tdt].cb = cb;
    _tx_meta[tdt].data = data;

    // Advanced TX descriptor
    _tx_ring[tdt].raw[0] = _mem.ptr_to_phys(data, len);
    _tx_ring[tdt].raw[1] = (0x2bU << 24 /* CMD: RS, DEXT, IFCS, EOP */)
      | (tx_desc::DTYP_DATA << 20) | len;
    
    asm ("" ::: "memory");

    _mmio[TDT0] = next_tdt;

    return true;
  }

  bool queue_buffer(uint8 *data, size_t len)
  {
    assert(len == 2048);

    unsigned rdt      = _mmio[RDT0];
    unsigned next_rdt = (rdt+1) % _ring_descs;
    if (next_rdt == _rx_to_clean)
      // RX queue full.
      return false;

    _rx_meta[rdt].data   = data;
    _rx_ring[rdt].raw[0] = _mem.ptr_to_phys(data, 2048);
    _rx_ring[rdt].raw[1] = 0;

    asm ("" ::: "memory");

    _mmio[RDT0] = next_rdt;
    //Logging::printf("Queue buffer %p at %x\n", data, rdt);

    return true;
  }

  bool poll_receive(uint8 **data, size_t *len)
  {
    if ((_rx_to_clean == _mmio[RDT0]) or
        ((_rx_ring[_rx_to_clean].rawd[2] & 1) == 0))
      return false;

    // Logging::printf("RCV %016llx %016llx\n",
    //                 _rx_ring[_rx_to_clean].raw[0],
    //                 _rx_ring[_rx_to_clean].raw[1]);

    // Descriptor done. Assert that this is a complete packet.
    assert((_rx_ring[_rx_to_clean].rawd[2] & 3) == 3);

    *data = _rx_meta[_rx_to_clean].data;
    *len  = _rx_ring[_rx_to_clean].rawd[3] & 0xFFFFU;

    _rx_to_clean = (_rx_to_clean + 1) % _ring_descs;

    return true;
  }

  void set_link(bool up)
  {
    uint32 enable = up ? (1U << 25) : 0;
    uint32 mask   = ~(1U << 25);
    _mmio[RXDCTL0] = (_mmio[RXDCTL0] & mask) | enable;
    _mmio[TXDCTL0] = (_mmio[TXDCTL0] & mask) | enable;
  }

  void enable_wakeups()
  {
    // Setup IRQ mapping:
    // RX and TX trigger MSI 0.
    // Mailbox IRQs are disabled.
    _mmio[VTIVAR]      = 0x00008080;
    _mmio[VTIVAR_MISC] = 0x00;

    // Setup autoclear and stuff for our IRQ.
    _mmio[VTEIAC] = 1;
    _mmio[VTEIAM] = 1;
    _mmio[VTEIMS] = 1;
  }

  void consume_wakeup()
  {
    // XXX Clear interrupt cause.
    _mmio[0xF4/4] = 0;

    // Cleanup descriptors and send notifications.
    while ((_tx_to_clean != _mmio[TDT0]) &&
           _tx_ring[_tx_to_clean].is_done()) {
      Logging::printf("Sent %x cb %p.\n", _tx_to_clean, _tx_meta[_tx_to_clean].cb);
      if (_tx_meta[_tx_to_clean].cb)
        _tx_meta[_tx_to_clean].cb->send_callback(_tx_meta[_tx_to_clean].data);
      _tx_to_clean = (_tx_to_clean + 1) % _ring_descs;
    }
  }

  void unmask_wakeups()
  {
    _mmio[VTEIMS] = 1;
  }

  SimpleNetworkClient(Memory &mem,
                      DBus<MessageVirtualNet> &bus_vnet)
    : _mem(mem), _tx_to_clean(0), _rx_to_clean(0)
  {
    _mmio    = _mem.allocate<uint32>(_mmio_size);
    _rx_ring = _mem.allocate<rx_desc>(_ring_size);
    _tx_ring = _mem.allocate<tx_desc>(_ring_size);

    _mmio[VTSTATUS] = 0x83U;
    _mmio[SRRCTL0]  = 1U << 25 /* Advanced RX, 1 Buffer */;

    uint64 rxbase = _mem.ptr_to_phys(_rx_ring, _ring_size);
    _mmio[RDBAL0] = rxbase;
    _mmio[RDBAH0] = rxbase >> 32;
    _mmio[RDLEN0] = _ring_size;

    uint64 txbase = _mem.ptr_to_phys(_tx_ring, _ring_size);
    _mmio[TDBAL0] = txbase;
    _mmio[TDBAH0] = txbase >> 32;
    _mmio[TDLEN0] = _ring_size;

    MessageVirtualNet vnetmsg(0, _mmio);
    if (not bus_vnet.send(vnetmsg))
      Logging::panic("Could not attach to virtual network.\n");

    Logging::printf("Network client is functional.\n");
  }
};


class VancouverMemory : public SimpleNetworkClient::Memory
{
  DBus<MessageHostOp> &_bus_hostop;
  mword offset;

public:

  virtual void *allocate_backend(size_t size)
  {
    MessageHostOp alloc(MessageHostOp::OP_ALLOC_FROM_GUEST, size);
    if (not _bus_hostop.send(alloc))
      Logging::panic("Could not allocate register window.\n");

    return reinterpret_cast<void *>(alloc.phys + offset);
  }

  virtual mword ptr_to_phys(const void *ptr, size_t size)
  {
    return reinterpret_cast<mword>(ptr) - offset;
  }

  VancouverMemory(DBus<MessageHostOp> &bus_hostop)
    : _bus_hostop(bus_hostop)
  {
    MessageHostOp conv(MessageHostOp::OP_GUEST_MEM, 0UL);
    if (not _bus_hostop.send(conv) or (conv.ptr == NULL))
      Logging::panic("Could not convert VM pointer?\n");
    offset = reinterpret_cast<mword>(conv.ptr);
  }
};

class NullMemory : public SimpleNetworkClient::Memory
{
public:
  
  virtual void *allocate_backend(size_t size)
  {
    return memalloc(size, 0x1000);
  }

  virtual mword ptr_to_phys(const void *ptr, size_t size)
  {
    return reinterpret_cast<mword>(ptr);
  }

  NullMemory() {}
};

// EOF
