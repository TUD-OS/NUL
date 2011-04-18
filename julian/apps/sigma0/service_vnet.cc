/*
 * Virtual Network Switch.
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

// STATUS
//
// This code is NOT PRODUCTION QUALITY.
//
// OVERVIEW
//
// This software switch is designed to connect 82576VF models only. It
// is implemented in a single thread running on a single CPU polling
// each client's queues round-robin to avoid locking overhead. It is
// currently not clear how well this approach scales, but related work
// has been encouraging:
// http://www.usenix.org/event/atc10/tech/full_papers/Shalev.pdf
//
// The switch has access to each client's DMA queues and physical
// memory and implements all of the 82576VF's packet processing and
// interrupt throttling features. Each client has its own notification
// semaphore, which the switch uses to signal an interrupt.
//
// CLIENT PERFORMANCE TUNING
// 
// Operating Systems may need tuning to achieve optimal throughput,
// especially for TCP connections. At least, Linux seems to autotune
// itself well, given enough RAM.
// 
// REMARKS
//
// The first version of this switch suffered from overengineering,
// because it tried to be generic with respect to the kind of models
// you can plug into it, because it created backpressure, and because
// it supported uplink ports with real NICs behind them. Every second
// call became virtual and it was extremely hard to follow the
// code. This second try is completely special purpose: It allows only
// downlink ports with a specific network model.

// TODO
// - give each TxQueue its own b0rken method
// - move deliver_*_from into RxQueue
// - better target cache


#include <nul/motherboard.h>
#include <nul/compiler.h>
#include <sys/semaphore.h>
#include <service/hexdump.h>
#include <service/net.h>
#include <service/endian.h>
#include <nul/net.h>

#include "target_cache.h"
#include "monitor.h"

using namespace Endian;

#define WORKUNITS     4
#define MAXPORT       16
#define MAXDESC       32
#define MAXHEADERSIZE 128
#define MAXPACKETSIZE 2048      // No Jumbo frames

template <typename T> T min(T a, T b) { return (a < b) ? a : b; }
template <typename T> T max(T a, T b) { return (b < a) ? a : b; }


class ALIGNED(16) VirtualNet : public StaticReceiver<VirtualNet>
{
  VnetMonitor                  _monitor;
  DBus<MessageVirtualNetPing> &_bus_vnetping;
  Clock                        _clock;

  struct Port {
    VirtualNet *vnet;

    uint32  *reg;
    mword    physsize;
    mword    physoffset;
    unsigned client;

    timevalue next_irq[3];

    enum RxReg {
      RDBAL  = 0x800/4,
      RDBAH  = 0x804/4,
      RDLEN  = 0x808/4,
      SRRCTL = 0x80C/4,
      RDH    = 0x810/4,
      RDT    = 0x818/4,
      RXDCTL = 0x828/4,
    };

    enum TxReg {
      TDBAL   = 0x800/4,
      TDBAH   = 0x804/4,
      TDLEN   = 0x808/4,
      TDH     = 0x810/4,
      TDT     = 0x818/4,
      TXDCTL  = 0x828/4,
      TDWBAL  = 0x838/4,
      TDWBAH  = 0x83C/4,
    };

    class TxQueue {
      uint32       *&reg;
    public:
      Port          &port;
      const unsigned no;

      /// Context descriptors are stored as-is until they are needed.
      tx_desc        ctx[8] ALIGNED(16);

      unsigned char  dma_prog_len; // Length of the DMA program in descriptors
      unsigned char  dma_prog_cur; // Current descriptor
      tx_desc        dma_prog[MAXDESC] ALIGNED(16);
      tx_desc       *dma_prog_out[MAXDESC];

      unsigned       dma_prog_cur_offset; // Bytes already consumed in the current descriptor

      Port          *tse_dest;  // NULL -> Broadcast
      tx_desc        tse_ctx;
      unsigned       tse_sent;

      /// Buffer used for broadcast packets instead of dma_prog
      uint8          packet_data[MAXPACKETSIZE];

      /// Bytes valid in packet_data. If zero, evaluate dma_prog instead.
      unsigned       packet_extracted;           
      
      uint32 &operator[] (TxReg offset)
      {
	assert((offset + 0x100*no) < 0x1000/4);
	return reg[offset + 0x3000/4 + 0x100*no];
      }

      // Descriptor write-back
      void writeback(unsigned dno)
      {
	//Logging::printf("WB %p %u TDH %x\n", d, force, (*this)[TDH]);
        bool irq = dma_prog[dno].rs();
        // XXX Always write back?
        dma_prog_out[dno]->set_done();
        if (irq) port.irq_reason(2*no + 1);
      }

      // The current DMA program has data pending.
      bool tx_data_pending()
      {
	return (dma_prog_cur != dma_prog_len);
      }

      // Copy raw packet data into local buffer. Don't copy more than
      // `size' bytes. Do writeback of TX descriptors.
      size_t data_in(uint8 *dest, size_t size, size_t acc = 0)
      {
	assert(dma_prog_cur < dma_prog_len);
	assert(dma_prog_cur_offset < dma_prog[dma_prog_cur].dtalen());
	//Logging::printf("%s: %p+%x (%x) TDH %x\n", __func__, dest, size, acc, (*this)[TDH]);
	size_t dtalen = dma_prog[dma_prog_cur].dtalen();
	size_t chunk = min<size_t>(size, dtalen - dma_prog_cur_offset);
	uint8 const *src = port.convert_ptr<uint8>(dma_prog[dma_prog_cur].raw[0] + dma_prog_cur_offset, chunk);
	if (src == NULL) { port.b0rken("data_in"); return 0; }
	memcpy(dest, src, chunk);
        port.vnet->_monitor.add<MONITOR_DATA_IN>(chunk);

	size -= chunk;
	dest += chunk;
	acc  += chunk;

	if (dma_prog_cur_offset + chunk < dtalen) {
	  // Current descriptor not complete.
	  assert(size == 0);
	  dma_prog_cur_offset += chunk;
	  return acc;
	} else {
	  // Consumed a full descriptor.
	  dma_prog_cur += 1;
	  dma_prog_cur_offset = 0;
	  writeback(dma_prog_cur-1);

	  if ((size == 0) or (dma_prog_cur == dma_prog_len))
	    return acc;
	  else
	    return data_in(dest, size, acc);
	}
      }

      void init_tse(Port *dest_port)
      {
        tse_dest        = dest_port;
        tse_ctx         = ctx[dma_prog[0].idx()];
        tse_sent        = 0;
        // XXX TSE
      }

      // Extract one packet to internal storage.
      void extract_to_buffer()
      {
#warning Needs to be adapted for TSE
        packet_extracted = data_in(packet_data, sizeof(packet_data));
        apply_offloads(ctx[dma_prog[0].idx()], dma_prog[0], packet_data, packet_extracted);
        
        if (packet_extracted < 60) {
          // Always pad small packets, disregarding PSP flag.
          memset(packet_data + packet_extracted, 0, 60 - packet_extracted);
          packet_extracted = 60;
        }

        assert(dma_prog_cur == dma_prog_len); // Consumed the whole program
      }

      void invalidate_buffer()
      {
        packet_extracted = 0;
      }
      
      void exec_dma_prog()
      {
	// For simplicity's sake, we need at least the ethernet header
	// in the first data block.
	if (dma_prog[0].dtalen() < 12) { port.b0rken("complex"); return; }

	uint8 const * const ethernet_header = port.convert_ptr<uint8>(dma_prog[0].raw[0], 12);
	if (ethernet_header == NULL) { port.b0rken("ehdr"); return; }

	EthernetAddr target(*reinterpret_cast<uint64 const * const>(ethernet_header));
	EthernetAddr source(*reinterpret_cast<uint64 const * const>(ethernet_header + 6));

	// Logging::printf("Packet from " MAC_FMT " to " MAC_FMT "\n",
        //                 MAC_SPLIT(&target), MAC_SPLIT(&source));

	VirtualNet * const vnet = port.vnet;
	Port       * const dest = vnet->_cache.lookup(target);
	if (!source.is_multicast()) vnet->_cache.remember(source, &port);

        bool tse = dma_prog[0].dcmd() & tx_desc::DCMD_TSE;
        bool unicast = (dest != NULL) and (dest->is_used());

        vnet->_monitor.add<MONITOR_PACKET_IN>(1);

        if (tse) init_tse(unicast ? dest : NULL);

	if (unicast) {
	  // Unicast
	  assert (dest != &port);
	  dest->deliver_from(*this);
	} else {
	  // Broadcast
          extract_to_buffer();
	  for (unsigned i = 0; i < MAXPORT; i++)
	     if (vnet->_port[i].is_used() and (&(vnet->_port[i]) != &port))
               vnet->_port[i].deliver_from(*this);
          invalidate_buffer();
	}
      }

      // Check for packets to be transmitted. Transmits a single packet.
      void tx()
      {
	// Queue disabled?
	if (not ((*this)[TXDCTL] & (1 << 25 /* ENABLE */))) {
	  // Logging::printf("TXDCTL %x (disabled)\n", (*this)[TXDCTL]);
	  // If you disable the queue, cancel pending TSE work.
	  dma_prog_cur = 0;
	  dma_prog_len = 0;
	  return;
	}

        if (next_dma_prog())
          exec_dma_prog();
      }

      /// Fetch a new DMA program. Returns true, iff successful.
      bool next_dma_prog()
      {
	TxQueue &txq = *this;
	uint32 tdh = txq[TDH];
	const uint32 tdt = txq[TDT];

	//Logging::printf("%014llx P%u TDH %03x TDT %03x F0 %03x F4 %03x\n", port.vnet->_clock.time() >> 8, port.vnet->port_no(port), tdh, tdt, reg[0xF0/4], reg[0xF4/4]);
	// No DMA descriptors?
	if (tdt == tdh) return false;

	tx_desc *queue
	  = port.convert_ptr<tx_desc>(static_cast<uint64>(txq[TDBAH] & ~0x7F)<<32 | txq[TDBAL],
				      txq[TDLEN]);

	if (queue == NULL) { port.b0rken("queue == NULL"); return false; }

	const unsigned queue_len = txq[TDLEN]/sizeof(tx_desc);

	// Logging::printf("%u descs to process\n",
	// 		      (tdh <= tdt) ? (tdt - tdh)
	// 		      : (queue_len + tdt - tdh));

	// Collect a complete DMA program. Store pointers to descriptors
	// to avoid expensive modulo operation later on.
	if (tdh >= queue_len) { port.b0rken("tdh >= queue_len"); return false; }

	dma_prog_cur = 0;
	dma_prog_len = 0;
	dma_prog_cur_offset = 0;

	// We have at least one DMA descriptor to process.
	do {
	  tx_desc &cur = queue[tdh];

	  //Logging::printf("-> TDH %x TDT %x LEN %x %x DTYP %u\n", tdh, tdt, queue_len, txq[TDLEN], cur.dtyp());
	
	  if (++tdh >= queue_len) tdh -= queue_len;
	  txq[TDH] = tdh;

	  // Consume context descriptors first.
	  if ((cur.dtyp() == tx_desc::DTYP_CONTEXT)) {
	    if (dma_prog_len != 0) {
	      // DMA program is broken. Context descriptors between data
	      // descriptors.
	      port.b0rken("DTA CTX DTA");
	      return false;
	    }

	    ctx[cur.idx()] = cur;
	  } else {
	    // DATA descriptor

            dma_prog_out[dma_prog_len] = &cur;
	    dma_prog[dma_prog_len]     = cur;
            dma_prog_len += 1;

	    if (dma_prog[dma_prog_len-1].eop())
	      // Successfully read a DMA program
	      goto handle_data;
	  }

	} while ((dma_prog_len < MAXDESC) and (tdt != tdh));
        Logging::printf("%u desc DMA program\n", dma_prog_len);

	if (dma_prog_len == 0)
	  // Only context descriptors were processed. Nothing left to
	  // do.
	  return false;

	// EOP is always true for context descriptors.
	if (not dma_prog[dma_prog_len-1].eop()) {
	  port.b0rken("EOP?"); return false;
	}

      handle_data:
	// Legacy descriptors are not implemented.
	if (dma_prog[0].legacy()) {
	  Logging::printf("LEGACY(%u): (TDH %u TDH %u)\n", dma_prog_len, txq[TDH], txq[TDT]);
	  for (unsigned i = 0; i < dma_prog_len; i++)
	    Logging::printf(" %016llx %016llx\n", dma_prog[i].raw[0], dma_prog[i].raw[1]);
	  return false;
	}

	uint8 dtyp = dma_prog[0].dtyp();
	//const tx_desc cctx = txq.ctx[txq.dma_prog[0]->idx()];

	//Logging::printf("REF%u = %016llx %016llx\n", dma_prog[0]->idx(), cctx.raw[0], cctx.raw[1]);
	assert(dtyp == tx_desc::DTYP_DATA);

        return true;
      }

      TxQueue(Port &port, uint32 *&reg, unsigned no)
        : reg(reg), port(port), no(no), ctx()
      { }
    };

    TxQueue tx0;
    TxQueue tx1;

    class RxQueue {
      uint32       *&reg;
    public:
      uint32 &operator[] (RxReg offset)
      {
	unsigned no = 0;
	assert((offset + 0x100*no) < 0x1000/4);
	return reg[offset + 0x2000/4 + 0x100*no];
      }

      RxQueue(uint32 *&reg) : reg(reg) { }
    };
    
    RxQueue rx0;

    bool is_used() const { return reg != NULL; }

    void b0rken(char const *msg) COLD
    {
      Logging::printf("Port is b0rken: %s.\n", msg);
      vnet->debug();
      reg = NULL;
    }

    template <typename T>
    T *convert_ptr(mword ptr, mword size) const
    {
      return (ptr <= (physsize - size)) ?
        reinterpret_cast<T *>(ptr + physoffset) : NULL;
    }

    // Execute a segmentation program. Return true, iff something was
    // delivered.
    bool deliver_tse_from(TxQueue &txq)
    {
      COUNTER_INC("deliver_tse");
      Logging::panic("TSE...");
      // XXX Complete...
      return false;
    }

    static void apply_offloads(const tx_desc ctx, const tx_desc d, uint8 *packet, size_t psize)
    {
      uint8 popts = d.popts();
      //Logging::printf("%016llx %016llx: POPTS %x\n", ctx.raw[0], ctx.raw[1], popts);
      // Short-Circuit return, if no interesting offloads are to be done.
      if ((popts & 7) == 0) return;

      uint16 tucmd = ctx.tucmd();      
      uint16 iplen = ctx.iplen();
      uint8 maclen = ctx.maclen();

      // Sanity check maclen and iplen. We only cover the case that is
      // harmful to us.
      if ((maclen+iplen > psize)) 
	return;

      if ((popts & 4) != 0 /* IPSEC */) {
#ifndef BENCHMARK
        Logging::printf("XXX IPsec offload requested. Not implemented!\n");
        // Since we don't do IPsec, we can skip the rest, too.
#endif
        return;
      }

      if (((popts & 1 /* IXSM     */) != 0) &&
          ((tucmd & 2 /* IPv4 CSO */) != 0)) {
	COUNTER_INC("IP offload");
	uint16 &ipv4_sum = *reinterpret_cast<uint16 *>(packet + maclen + 10);
	ipv4_sum = 0;
	ipv4_sum = IPChecksum::ipsum(packet, maclen, iplen);
	//Logging::printf("IPv4 CSO: %x\n", ipv4_sum);
      }

      if ((popts & 2 /* TXSM */) != 0) {
        // L4 offload requested. Figure out packet type.
        uint8 l4t = (tucmd >> 2) & 3;

        switch (l4t) {
        case tx_desc::L4T_UDP:		// UDP
        case tx_desc::L4T_TCP:		// TCP
          {
	    COUNTER_INC("L4 offload");
            uint8 *l4_sum = packet + maclen + iplen + ((l4t == tx_desc::L4T_UDP) ? 6 : 16);
            l4_sum[0] = l4_sum[1] = 0;
            uint16 sum = IPChecksum::tcpudpsum(packet, (l4t == tx_desc::L4T_UDP) ? 17 : 6, maclen, iplen, psize);
	    l4_sum[0] = sum;
	    l4_sum[1] = sum>>8;
	    //Logging::printf("%s CSO %x\n", (l4t == tx_desc::L4T_UDP) ? "UDP" : "TCP", sum);
          }
          break;
#ifndef BENCHMARK
        case tx_desc::L4T_SCTP:		// SCTP
          // XXX Not implemented.
          Logging::printf("XXX SCTP CSO requested. Not implemented!\n");
          break;
#endif
        case 3:
          // Invalid. Nothing to be done.
          break;
        }
      }
    }

    // Deliver a simple packet to this port. Returns true, iff
    // something was delivered.
    // XXX Cleanup/Refactor
    bool deliver_simple_from(TxQueue &txq)
    {
      uint32 rdh   = rx0[RDH];
      uint32 rdlen = rx0[RDLEN];

      COUNTER_INC("deliver_sim");

      if (rdh*sizeof(rx_desc) >= rdlen) { b0rken("rdh >= rdlen"); return false; }

      //Logging::printf("RDH %u\n", rdh);

      rx_desc *rx_queue = convert_ptr<rx_desc>(static_cast<uint64>(rx0[RDBAH] & ~0x7F)<<32 | rx0[RDBAL],
                                               rx0[RDLEN]);
      if (rx_queue == NULL) { b0rken("rx == NULL"); return false; }
      rx_desc &rx = rx_queue[rdh];

      uint8 desc_type    = (rx0[SRRCTL] >> 25) & 0x7;
      size_t buffer_size = (rx0[SRRCTL] & 0x7F) * 1024;
      if (buffer_size == 0) buffer_size = 2048;

      unsigned packet_extracted = txq.packet_extracted;

      if (not packet_extracted) {
        // Direct path, packet not extracted. Zero Copy!
        while (txq.tx_data_pending()) {
          rx = rx_queue[rdh];
          uint8 *data = convert_ptr<uint8>(rx.raw[0], buffer_size);
          if (data == NULL) { b0rken("data == NULL"); return false; }
          
          uint16 psize = txq.data_in(data, buffer_size);
          vnet->_monitor.add<MONITOR_DATA_OUT>(psize);
          
#warning XXX offloads broken when packet spread over multiple descriptors
          apply_offloads(txq.ctx[txq.dma_prog[0].idx()], txq.dma_prog[0], data, psize);
          //Logging::printf("Received %u bytes.\n", psize);

          if (desc_type >> 1) {
            Logging::printf("srrctl %08x\n", rx0[SRRCTL]);
            vnet->debug();
          }

          // XXX Very very evil and slightly b0rken small packet padding.
#warning Implement PSP
          if (not txq.tx_data_pending() && (psize < 60))
            psize = 60;
          
          rx.set_done(desc_type, psize, not txq.tx_data_pending());
          vnet->_monitor.add<MONITOR_PACKET_OUT>(1);
          
          rdh += 1;
          if (rdh * sizeof(rx_desc) >= rdlen) rdh -= rdlen/sizeof(rx_desc);
          rx0[RDH] = rdh;
        }
      } else {
        // Packet extracted. PSP&offloads are already done. Slow
        // broadcast.
        uint8 *packet = txq.packet_data;

        vnet->_monitor.add<MONITOR_DATA_OUT>(packet_extracted);
        vnet->_monitor.add<MONITOR_PACKET_OUT>(1);

        do {
          rx = rx_queue[rdh];
          uint8 *data = convert_ptr<uint8>(rx.raw[0], buffer_size);
          if (data == NULL) { b0rken("data == NULL"); return false; }
          unsigned chunk = min(buffer_size, packet_extracted);
          memcpy(data, packet, chunk);
          packet += chunk;
          packet_extracted -= chunk;

          rx.set_done(desc_type, chunk, packet_extracted == 0);
          
          rdh += 1;
          if (rdh * sizeof(rx_desc) >= rdlen) rdh -= rdlen/sizeof(rx_desc);
          rx0[RDH] = rdh;
        } while (packet_extracted);
      }

      return true;
    }

    // Look up the MSI-X vector corresponding to `n' and set the bit
    // in EICR.
    void irq_reason(unsigned n)
    {
      uint8 vector = VTIVAR() >> (n*8);
      //Logging::printf("REASON %u VECTOR %u\n", n, vector);
      if (vector & 0x80) {
	schedule_irq(1U << (vector & 0x3));
      }
    }

    // Deliver a packet to `this` port.
    void deliver_from(TxQueue &txq)
    {
      // Queue enabled?
      if ((rx0[RXDCTL] & (1U << 25)) == 0) {
	// Logging::printf("Packet dropped. Queue %u disabled.\n", vnet->port_no(this));
	// vnet->debug();
	return;
      }

      bool res;
      if (txq.dma_prog[0].dcmd() & tx_desc::DCMD_TSE)
	res = deliver_tse_from(txq);
      else
	res = deliver_simple_from(txq);

      if (res) irq_reason(0);
    }

    uint32 &VTIVAR() { return reg[0x1700/4]; }
    uint32 &VTEICR() { return reg[0x1580/4]; }
    uint32 &VTEIMS() { return reg[0x1524/4]; }
    uint32 &VTEIAC() { return reg[0x152c/4]; }
    uint32 &VTEIAM() { return reg[0x1530/4]; }
    uint32 &VTEITR(unsigned u) { return reg[0x1680/4 + u]; }

    void schedule_irq(uint32 ics)
    {
      Cpu::atomic_or(&VTEICR(), ics);
      Cpu::atomic_or(&reg[0xF0/4], ics);
      //Logging::printf("SCHED ICR %02x MSK %02x NEW %02x\n", VTEICR(), VTEIMS(), reg[0xF0/4]);
    }

    // Check if IRQs have to be sent. Returns true, iff interrupts are
    // pending, but could not be sent yet.
    bool irq(Clock &clock, timevalue now)
    {
      uint32 effective_icr = VTEICR() & VTEIMS() & reg[0xF0/4];
      uint32 irqs_pending  = 0;
      bool pending_but_deferred = false;

      // if (effective_icr)
      // 	Logging::printf("ICR %02x IMS %02x NEW %02x INJ %02x\n",
      // 			VTEICR(), VTEIMS(), reg[0xF0/4], reg[0xF4/4]);

      for (unsigned i = 0; i < 3; i++) {
	if ((effective_icr & (1 << i)) == 0) continue;
	
	uint16 iv  = (VTEITR(i) >> 2) & 0xFFF;

	// If an interval is set (moderation is enabled), check
	// whether we are allowed to inject an interrupt at this
	// time.

	// XXX We don't update ITR registers at the moment, as no one
	// seem to use these values. Is this problematic?
        // XXX Maybe Linux reads them. I have to check that.
	if (iv != 0) {
	  if (next_irq[i] <= now) {
	    // Clear the counter. Indicates that the interrupt is
	    // pending.
	    //Cpu::atomic_and(&VTEITR(i), ~(0x3FFU << 21));
	    next_irq[i] = clock.abstime(iv, 1000000);
	  } else {
	    // Computing the remaining time for interrupt coalescing
	    // is quite expensive and no one uses the result. Set
	    // the time remaining to the interval instead.
	    // uint32 old_itr;
	    // uint32 new_itr;
	    // do {
	    //   old_itr = VTEITR(i);
	    //   new_itr = (old_itr & ~(0x3FFFU << 21)) | ((iv >> 2) << 21);
	    // } while (old_itr != Cpu::cmpxchg4b(&VTEITR(i), old_itr, new_itr));

	    // Defer sending this IRQ.
	    pending_but_deferred = true;
	    continue;
	  }
	}

	// Either throttling is not enabled or it is time to send
	// the IRQ.
	  
	irqs_pending |= (1 << i);
      }

      if (irqs_pending) {
	Cpu::atomic_and(&reg[0xF0/4], ~irqs_pending);
	Cpu::atomic_or(&reg[0xF4/4], irqs_pending);

	// Auto-Clear
	Cpu::atomic_and(&VTEIMS(), ~(VTEIAM() & irqs_pending));
	Cpu::atomic_and(&VTEICR(), ~(VTEIAC() & irqs_pending));

	COUNTER_INC("ping");
	MessageVirtualNetPing p(client);
	vnet->_bus_vnetping.send(p);
      }
      
      return pending_but_deferred;
    }

    void tx()
    {
      tx0.tx();
      //tx1.tx();
    }

    Port() : reg(NULL), next_irq(), tx0(*this, reg, 0), tx1(*this, reg, 1), rx0(reg)
    {}
  } _port[MAXPORT];

  TargetCache<8, VirtualNet::Port> _cache;

  unsigned port_no(Port const &p) { return &p - _port; };

  void debug()
  {
    for (unsigned i = 0; i < MAXPORT; i++) {
      Logging::printf("Port %02u:\n", i);
      Logging::printf("     vnet     %p (should be %p)\n", _port[i].vnet, this);
      Logging::printf("     reg      %p\n", _port[i].reg);
      Logging::printf("     physsize %08lx\n", _port[i].physsize);
      Logging::printf("     physoff  %08lx\n", _port[i].physoffset);
      Logging::printf("     client   %u\n", _port[i].client);

    }
  }

  void check_tx()
  {
    for (unsigned i = 0; i < MAXPORT; i++)
      if (_port[i].is_used()) _port[i].tx();
  }

  void check_irqs()
  {
    timevalue now = _clock.time();
    bool deferred_irq_pending = false;

    for (unsigned i = 0; i < MAXPORT; i++)
      if (_port[i].is_used() && _port[i].irq(_clock, now))
	deferred_irq_pending = true;

    // XXX Use deferred_irq_pending together with activity on the TX
    // queues to decide whether we should sleep.
  }

  NORETURN void work()
  {
    Logging::printf("VNet running.\n");

    while (1) {
      while (true) {
	// Do a constant amount of work here.
	for (unsigned i = 0; i < WORKUNITS; i++)
	  check_tx();
	check_irqs();
      }

      // NOTREACHED
    }
  }

  static void do_work(void *t) REGPARM(0) NORETURN 
  {
    reinterpret_cast<VirtualNet *>(t)->work();
  }

public:

  bool receive(MessageVirtualNet &msg)
  {
    switch (msg.op) {
    case MessageVirtualNet::ANNOUNCE:
      for (unsigned i = 0; i < MAXPORT; i++) {
        if (not _port[i].is_used()) {
          Logging::printf("VNET attached port %u. Memory @ %08lx+%lx\n", i,
                          msg.physoffset, msg.physsize);
	  if (msg.registers[2] != 0x83U) {
	    Logging::printf("Wrong window mapped! %p %x\n", msg.registers,
			    msg.registers[2]);
	    return false;
	  }

	  _port[i].client     = msg.client;
          _port[i].physoffset = msg.physoffset;
          _port[i].physsize   = msg.physsize;
          MEMORY_BARRIER;
          _port[i].reg        = msg.registers;
          return true;;
        }
      }
      break;
    default:
      break;
    }
    return false;
  }

  VirtualNet(Motherboard &mb)
    : _monitor(mb.bus_hostop), _bus_vnetping(mb.bus_vnetping), _clock(*mb.clock()), _port()
  {
    for (unsigned i = 0; i < MAXPORT; i++)
      _port[i].vnet = this;

    mb.bus_vnet.add(this, receive_static<MessageVirtualNet>);

    MessageHostOp msg2 = MessageHostOp::alloc_service_thread(VirtualNet::do_work,
                                                             this);
    if (!mb.bus_hostop.send(msg2))
      Logging::panic("%s alloc service thread failed.", __func__);
  }

};

PARAM(vnet,
      VirtualNet *n = new(16) VirtualNet(mb);
      assert((reinterpret_cast<mword>(n) & 0xF) == 0);
      ,
      "vnet - virtual network switch");

// EOF
