// -*- Mode: C++ -*-
/** @file
 * Intel 82576 VF device model.
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

#include <nul/types.h>
#include <nul/motherboard.h>
#include <service/hexdump.h>
#include <service/time.h>
#include <service/net.h>
#include <service/endian.h>
#include <nul/net.h>
#include <model/pci.h>

#include "82576vf.h"

using namespace Endian;

// Status: INCOMPLETE (but working for Linux)
// 
// This model supports two modes of operation for the TX path:
//  - trap&emulate mode (default):
//     trap every access to TX registers
//  - polled mode:
//     check every n µs for queued packets. n is configured using
//     the txpoll_us parameter (see the comment at the bottom of
//     this file).

// TODO
// - handle BAR remapping
// - RXDCTL.enable (bit 25) may be racy
// - receive path does not set packet type in RX descriptor
// - TX legacy descriptors
// - interrupt thresholds
// - don't copy packet on TX path if offloading is not used
// - fancy offloads (SCTP CSO, IPsec, ...)
// - CSO support with TX legacy descriptors
// - scatter/gather support in MessageNetwork to avoid packet copy in
//   TX path.

class Model82576vf : public StaticReceiver<Model82576vf>
{
  EthernetAddr           _mac;
  DBus<MessageNetwork>  &_net;
#include "model/simplemem.h"
  Clock                 *_clock;
  DBus<MessageTimer>    &_timer;
  unsigned               _timer_nr;

  // Guest-physical addresses for MMIO and MSI-X regs.
  uint32 _mem_mmio;
  uint32 _mem_msix;

  // Two pages of memory holding RX and TX registers.
  uint32 *_local_rx_regs;	// Mapped to _mem_mmio + 0x2000
  uint32 *_local_tx_regs;	// Mapped to _mem_mmio + 0x3000

  
  // TX queue polling interval in µs.
  unsigned _txpoll_us;

  // Map RX registers?
  bool _map_rx;
  unsigned _bdf;

  // Filtering
  const bool _promisc_default;
  bool       _promisc;
  Mta        _mta;

#include <model/82576vfmmio.inc>
#include <model/82576vfpci.inc>

  struct queue {
    Model82576vf *parent;
    unsigned n;
    volatile uint32 *regs;

    virtual void reset() = 0;

    void init(Model82576vf *_parent, unsigned _n, uint32 *_regs)
    {
      parent = _parent;
      n      = _n;
      regs   = _regs;

      reset();
    }
  };
  
  struct tx_queue : queue {
    uint32 txdctl_old;

    // The driver can program 8 different offload contexts. We buffer
    // them as-is until they are needed.
    tx_desc ctx[8];

    enum {
      TDBAL   = 0x800/4,
      TDBAH   = 0x804/4,
      TDLEN   = 0x808/4,
      TDH     = 0x810/4,
      TDT     = 0x818/4,
      TXDCTL  = 0x828/4,
      TDWBAL  = 0x838/4,
      TDWBAH  = 0x83C/4,
    };

    // We use a huge buffer, because the VM may use segmentation
    // offload and put a whole TCP window worth of data here.
    uint8 packet_buf[64 * 1024];
    unsigned packet_cur;

    void reset()
    {
      memset(const_cast<uint32 *>(regs), 0, 0x100);
      regs[TXDCTL] = (n == 0) ? (1<<25) : 0;
      txdctl_old = regs[TXDCTL];
      packet_cur = 0;

      regs[TDBAL] = 0;
      regs[TDBAH] = 0;
      regs[TDLEN] = 0;
      regs[TDT]   = 0;
      regs[TDH]   = 0;
    }

    void txdctl_poll()
    {
      uint32 txdctl_new = regs[TXDCTL];
      if (((txdctl_old ^ txdctl_new) & (1<<25)) != 0) {
	// Enable/Disable receive queue
	Logging::printf("TX queue %u: %s\n", n,
			((txdctl_new & (1<<25)) != 0) ? "ENABLED" : "DISABLED");
      }
      regs[TXDCTL] &= ~(1<<26);	// Clear SWFLUSH
      txdctl_old = txdctl_new;
    }
    
    void handle_ctx(uint64 addr, tx_desc &desc)
    {
      // Store context descriptor as is, evaluate it when we need it.
      ctx[desc.idx()] = desc;
    }

    void apply_segmentation(uint8 *packet, uint32 packet_len,
			    const tx_desc &desc, bool tse)
    {
      uint32 payload_len = desc.paylen();

      if (!tse) {
	// Skip segmentation if it is not requested.
	if (payload_len != packet_len) {
	  Logging::printf("XXX Got %x bytes, but payload size is %x. Huh? Ignoring packet.\n", packet_len, payload_len);
	  return;
	}
	apply_offload(packet, payload_len, desc);
        MessageNetwork m(packet, packet_len, 0);
	parent->_net.send(m);
      } else {
	// TCP segmentation is a bit weird, because the payload length
	// in the TX descriptor does not include the prototype header.

	uint8  cc     = desc.idx();
	const tx_desc &cur_ctx = ctx[cc];
	uint16 tucmd  = cur_ctx.tucmd();
        uint8  l4t    = (tucmd >> 2) & 3;
	bool   ipv6   = ((tucmd & 2) == 0);
	//uint8  l4len  = (cur_ctx.raw[1]>>40) & 0xFF;
	uint16 mss    = (cur_ctx.raw[1]>>48) & 0xFFFF;
	uint16 iplen  =  cur_ctx.raw[0];
	uint8  maclen = (iplen >> 9) & 0xFF;
	iplen &= 0x1FF;
	uint32 header_len = packet_len - payload_len;
	uint32 data_left = payload_len;
	uint32 data_sent = 0;

	//Logging::printf("SEGMENT header %x data %x (total %x) mss %x\n", header_len, data_left, packet_len, mss);

	if (l4t == tx_desc::L4T_SCTP) {
	  Logging::printf("XXX SCTP segmentation?\n");
	  return;
	}

	if (l4t == tx_desc::L4T_UDP) {
	  Logging::printf("XXX UDP segmentation not implemented.\n");
	  return;
	}

	// Logging::printf("SEGMENT HEADERS: MAC %x IP %x L4 %x %s%s\n", maclen, iplen, l4len,
	// 		(l4t == L4T_UDP) ? "UDP" : "TCP", ipv6 ? "v6" : "v4");

	//hexdump(packet, packet_len);

	//uint16 &packet_mac_len = *(uint16 *)(packet + 6 + 6);
	uint16 &packet_ip4_id  = *reinterpret_cast<uint16 *>(packet + maclen + 4);
	uint16 &packet_ip_len  = *reinterpret_cast<uint16 *>(packet + maclen + (ipv6 ? 4 : 2));
	uint32 &packet_tcp_seq = *reinterpret_cast<uint32 *>(packet + maclen + iplen + 4);
	uint8  &packet_tcp_flg = packet[maclen + iplen + 13];
	uint8  tcp_orig_flg    = packet_tcp_flg;

	unsigned i = 0;
	while (data_left > 0) {
	  uint16 chunk_size = (data_left > mss) ? mss : data_left;
	  data_left -= chunk_size;
	  //Logging::printf("CHUNK%u: DTA %x \n", i, chunk_size);
	  
	  // XXX ?
	  //packet_mac_len = chunk_size + maclen + iplen + l4len - 14;

	  // Logging::printf("IP len %x (at %x)\n", chunk_size + header_len - maclen,
	  // 		  maclen + (ipv6 ? 4 : 2));
	  packet_ip_len = hton16(chunk_size + header_len - maclen);

	  // XXX UDP

	  if (l4t == tx_desc::L4T_TCP)
	    packet_tcp_flg = tcp_orig_flg &
	      ((data_left == 0) ? /* last */ 0xFF : /* intermediate: set FIN/PSH */ ~9);

	  // Move packet data
	  if (data_sent != 0) memmove(packet + header_len,
				      packet + header_len + data_sent,
				      chunk_size);
	  
	  // At this point we have prepared the final packet, we just
	  // need to fix checksums and off it goes...
	  uint32 segment_len = header_len + chunk_size;
	  //Logging::printf("CHUNK%u sent %x bytes\n", i-1, segment_len);
	  //hexdump(packet, segment_len);
	  apply_offload(packet, segment_len, desc);
	  MessageNetwork m(packet, segment_len, 0);
	  parent->_net.send(m);

	  // Prepare next chunk
	  data_sent += chunk_size;
	  if (!ipv6) packet_ip4_id = hton16(ntoh16(packet_ip4_id) + 1);
	  if (l4t == tx_desc::L4T_TCP) packet_tcp_seq = hton32(ntoh32(packet_tcp_seq) + chunk_size);
	  i++;
	}
	//Logging::panic("INSPECT");
	// 
      }
    }

    void apply_offload(uint8 *packet, uint32 packet_len,
                       const tx_desc &tx_desc)
    {
      uint8 popts = tx_desc.popts();
      // Short-Circuit return, if no interesting offloads are to be done.
      if ((popts & 7) == 0) return;

      unsigned cc  = tx_desc.idx();
      uint16 tucmd = ctx[cc].tucmd();
      uint16 iplen = ctx[cc].iplen();
      uint8 maclen = ctx[cc].maclen();

      // Sanity check maclen and iplen. We only cover the case that is
      // harmful to us.
      if ((maclen+iplen > packet_len)) 
	return;

      if ((popts & 4) != 0 /* IPSEC */) {
        Logging::printf("XXX IPsec offload requested. Not implemented!\n");
        // Since we don't do IPsec, we can skip the rest, too.
        return;
      }

      if (((popts & 1 /* IXSM     */) != 0) &&
          ((tucmd & 2 /* IPv4 CSO */) != 0)) {
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
            uint8 *l4_sum = packet + maclen + iplen + ((l4t == tx_desc::L4T_UDP) ? 6 : 16);
            l4_sum[0] = l4_sum[1] = 0;
            uint16 sum = IPChecksum::tcpudpsum(packet, (l4t == tx_desc::L4T_UDP) ? 17 : 6, maclen, iplen, packet_len,
                                               (tucmd & 2 /* IPv4 */) == 0);
	    l4_sum[0] = sum;
	    l4_sum[1] = sum>>8;
	    //Logging::printf("%s CSO %x\n", (l4t == L4T_UDP) ? "UDP" : "TCP", sum);
          }
          break;
        case tx_desc::L4T_SCTP:		// SCTP
          // XXX Not implemented.
          Logging::printf("XXX SCTP CSO requested. Not implemented!\n");
          break;
        case 3:
          // Invalid. Nothing to be done.
          break;
        }
      }
    }

    void handle_dta(uint64 addr, tx_desc &desc)
    {
      // uint32 payload_len = desc.advanced.pay >> 14;
      uint32 data_len = desc.dtalen();
      uint8  dcmd = desc.dcmd();
      // uint8  cc   = (desc.advanced.pay>>4) & 0x7;
      // Logging::printf("TX advanced data descriptor: dcmd %x dta %x pay %x ctx %x\n",
      // 		      dcmd, data_len, payload_len, cc);
      if ((dcmd & (1<<5)) == 0) {
        //Logging::printf("TX bad descriptor\n");
	return;
      }

      enum {
        EOP = 1,
        IFCS = 2,
        RS = 8,
        VLE = 64,
        TSE = 128,
      };

      // Logging::printf("%s%s%s%s%s\n", (dcmd&EOP)?"EOP ":"", (dcmd&IFCS)?"IFCS ":"", (dcmd&RS)?"RS ":"",
      // 		      (dcmd&VLE)?"VLE ":"", (dcmd&TSE)?"TSE":"");
      const uint8 *data = reinterpret_cast<uint8 *>(parent->guestmem(desc.raw[0]));

      if ((dcmd & IFCS) == 0)
        Logging::printf("IFCS not set, but we append FCS anyway in host82576vf.\n");

      // XXX Check if offloading is used. If not -> send directly from
      // guestmem.
      if ((packet_cur + data_len) > sizeof(packet_buf)) {
	Logging::printf("XXX Packet buffer too small? Skipping packet\n");
	packet_cur = 0;
	goto done;
      }

      memcpy(packet_buf + packet_cur, data, data_len);
      packet_cur += data_len;

      if (dcmd & EOP) {
	apply_segmentation(packet_buf, packet_cur, desc, (dcmd & TSE) != 0);
        packet_cur = 0;
      }

    done:
      // Descriptor is done
      desc.set_done();
      parent->copy_out(addr, desc.raw, sizeof(desc));
      if ((dcmd & (1<<3) /* Report Status */) != 0)
        parent->TX_irq(n);
    }

    void tdt_poll()
    {
      if ((regs[TXDCTL] & (1<<25)) == 0) {
	//if (n == 0) Logging::printf("TX: Queue %u not enabled.\n", n);
	return;
      }
      uint32 tdlen = regs[TDLEN];
      if (tdlen == 0) {
	//if (n == 0) Logging::printf("TX: Queue %u has zero size.\n", n);
	return;
      }

      uint32 tdbah = regs[TDBAH];
      uint32 tdbal = regs[TDBAL];

      // Packet send loop.
      uint32 tdh;
      while ((tdh = regs[TDH]) != regs[TDT]) {
	uint64 addr = (static_cast<uint64>(tdbah)<<32 | tdbal) + ((tdh*16) % tdlen);
	tx_desc desc;

	//Logging::printf("TX descriptor at %llx\n", addr);
	if (!parent->copy_in(addr, desc.raw, sizeof(desc)))
	  return;
	if ((desc.raw[1] & (1<<29)) == 0) {
	  Logging::printf("TX legacy descriptor: XXX\n");
	  // XXX Not implemented!
	} else {
	  uint8 dtyp = (desc.raw[1] >> 20) & 0xF;
	  switch (dtyp) {
	  case 2: handle_ctx(addr, desc); break;
	  case 3: handle_dta(addr, desc); break;
	  default:
	    Logging::printf("TX unknown descriptor?\n");
	  }
	}
	  
	// Advance queue head
	MEMORY_BARRIER;
	regs[TDH] = (((tdh+1)*16 ) % tdlen) / 16;
      }
    }

    uint32 read(uint32 offset)
    {
      // Logging::printf("TX read %x (%x)\n", offset, (offset & 0x8FF) / 4);
      return regs[(offset & 0x8FF)/4];
    }

    void write(uint32 offset, uint32 val)
    {
      // Logging::printf("TX write %x (%x) <- %x\n", offset, (offset & 0x8FF) / 4, val);
      unsigned i = (offset & 0x8FF) / 4;
      regs[i] = val;
      if (i == TXDCTL) txdctl_poll();
      if (i == TDT) tdt_poll();
      
    }

  };

  struct rx_queue : queue {
    uint32 rxdctl_old;

    typedef union {
      uint64 raw[2];
      struct {
	uint64 buffer;
	uint32 sumlen;
	uint32 status;
      } legacy;
      struct {
	uint64 pbuffer;
	uint64 hbuffer;
      } advanced_read;
      struct {
	uint32 info;
	uint32 rss_hash;
	uint32 status;
	uint16 len;
	uint16 vlan;
      } advanced_write;
    } rx_desc;

    enum {
      RDBAL  = 0x800/4,
      RDBAH  = 0x804/4,
      RDLEN  = 0x808/4,
      SRRCTL = 0x80C/4,
      RDH    = 0x810/4,
      RDT    = 0x818/4,
      RXDCTL = 0x828/4,
    };

    void reset()
    {
      memset(const_cast<uint32 *>(regs), 0, 0x100);
      regs[RXDCTL] = 1<<16 | ((n == 0) ? (1<<25) : 0);
      rxdctl_old = regs[RXDCTL];

      regs[RDBAL]  = 0;
      regs[RDBAH]  = 0;
      regs[RDLEN]  = 0;
      regs[SRRCTL] = (4U<<8) | ((n == 0) ? 0 : (1U<<31));
      regs[RDT]    = 0;
      regs[RDH]    = 0;
    }

    uint32 read(uint32 offset)
    {
      return regs[(offset & 0x8FF)/4];
    }

    void write(uint32 offset, uint32 val)
    {
      unsigned i = (offset & 0x8FF) / 4;
      regs[i] = val;
      if (i == RXDCTL) rxdctl_poll();
    }

    void rxdctl_poll()
    {
      uint32 rxdctl_new = regs[RXDCTL];
      if (((rxdctl_old ^ rxdctl_new) & (1<<25)) != 0) {
	// Enable/Disable receive queue
	Logging::printf("RX queue %u: %s\n", n,
			((rxdctl_new & (1<<25)) != 0) ? "ENABLED" : "DISABLED");
      }
      rxdctl_old = rxdctl_new;
    }

    void receive_packet(uint8 *buf, size_t size)
    {
      // Check early if this packet is for us.

      const EthernetAddr &dst = *reinterpret_cast<const EthernetAddr *>(buf);
      if (!parent->_promisc && !dst.is_broadcast() && !(dst == parent->_mac) &&
	  // XXX Check the MTA only for multicast MACs?
	  !parent->_mta.includes(dst)) {
	// Logging::printf("Dropping packet to " MAC_FMT " (%04x) (" MAC_FMT ")\n",
	//  		MAC_SPLIT(&dst), parent->_mta.hash(dst),
	//  		MAC_SPLIT(&parent->_mac));
	return;
      }

      rxdctl_poll();

      uint32 rdbah  = regs[RDBAH];
      uint32 rdbal  = regs[RDBAL];
      uint32 rdlen  = regs[RDLEN];
      uint32 srrctl = regs[SRRCTL];
      uint32 rdh    = regs[RDH];
      uint32 rdt    = regs[RDT];
      uint32 rxdctl = regs[RXDCTL];

      if (((rxdctl & (1<<25)) == 0 /* Queue disabled? */)
	  || (rdlen == 0) || (rdt == rdh)) {
	// Drop
      	return;
      }

      //Logging::printf("RECV %08x %08x %04x %04x\n", rdbal, rdlen, rdt, rdh);
      uint64 addr = (static_cast<uint64>(rdbah)<<32 | rdbal) + ((rdh*16) % rdlen);
      rx_desc desc;

      //Logging::printf("RX descriptor at %llx\n", addr);
      if (!parent->copy_in(addr, desc.raw, sizeof(desc)))
	return;

      // Which descriptor type?
      uint8 desc_type = (srrctl >> 25) & 0xF;
      switch (desc_type) {
      case 0:			// Legacy
       	{
       	  desc.legacy.status = 0;
       	  if(!parent->copy_out(desc.legacy.buffer, buf, size))
       	    desc.legacy.status |= 0x8000; // RX error
       	  desc.legacy.sumlen = size;
          MEMORY_BARRIER;
       	  desc.legacy.status |= 0x3; // EOP, DD
       	}
       	break;
      case 1:			// Advanced, one buffer
	{
	  uint64 target_buf = desc.advanced_read.pbuffer;
	  desc.advanced_write.rss_hash = 0;
	  desc.advanced_write.info = 0;
	  desc.advanced_write.vlan = 0;
	  desc.advanced_write.len = size;
	  if (!parent->copy_out(target_buf, buf, size))
       	    desc.advanced_write.status |= 0x80000000U; // RX error
	  MEMORY_BARRIER;
	  desc.advanced_write.status = 0x3; // EOP, DD
	}
	break;
      default:
	 Logging::printf("Invalid descriptor type %x\n", desc_type);
	 break;
      }
      
      if (!parent->copy_out(addr, desc.raw, sizeof(desc)))
	Logging::printf("RX descriptor writeback failed.\n");

      // Advance queue head
      MEMORY_BARRIER;
      regs[RDH] = (((rdh+1)*16 ) % rdlen) / 16;

      parent->RX_irq(n);
    }
  };
  
  tx_queue _tx_queues[2];
  rx_queue _rx_queues[2];

  // Software interface
  enum MBX {
    VF_RESET         = 0x0001U,
    VF_SET_MAC_ADDR  = 0x0002U,
    VF_SET_MULTICAST = 0x0003U,
    VF_SET_LPE       = 0x0005U,
    VF_SET_PROMISC   = 0x0006U,

    VF_SET_PROMISC_UNICAST = 0x04<<16,

    CMD_ACK          = 0x80000000U,
    CMD_NACK         = 0x40000000U,
    CTS              = 0x20000000U,
  };

  union {
    struct msix_table {
      uint64 msg_addr;
      uint32 msg_data;
      uint32 vector_control;
    } table[3];
    uint32 raw[3*4];
  } _msix;

  uint32 VTFRTIMER_compute()
  {
    // XXX
    return 0;
  }

  void *guestmem(uint64 addr)
  {
    MessageMemRegion msg(addr >> 12);
    if (!_bus_memregion->send(msg) || !msg.ptr)
      Logging::panic("Address translation failed.\n");
    return msg.ptr + addr - (msg.start_page << 12);
  }

  // Generate a MSI-X IRQ.
  void MSIX_irq(unsigned nr)
  {
    // Logging::printf("MSI-X IRQ %d | EIMS %02x | EIAC %02x | EIAM %02x | C %02x\n", nr,
    // 		    rVTEIMS, rVTEIAC, rVTEIAM, _msix.table[nr].vector_control);
    uint32 mask = 1<<nr;
    // Set interrupt cause.
    rVTEICR |= mask;

    if ((mask & rVTEIMS) != 0) {
      if ((_msix.table[nr].vector_control & 1) == 0) {
	// Logging::printf("Generating MSI-X IRQ %d (%02x)\n", nr, _msix.table[nr].msg_data & 0xFF);
	MessageMem msg(false, _msix.table[nr].msg_addr, &_msix.table[nr].msg_data);
	_bus_mem->send(msg);

	// Auto-Clear
	// XXX Do we auto-clear even if the interrupt cause was masked?
	// The spec is not clear on this. At least not to me...
	rVTEICR &= ~(mask & rVTEIAC);
	rVTEIMS &= ~(mask & rVTEIAM);
	// Logging::printf("MSI-X -> EIMS %02x\n", rVTEIMS);
      }
    }
  }

  /// Generate a mailbox/misc IRQ.
  void MISC_irq()
  {
    if ((rVTIVAR_MISC & 0x80) && ((rVTIVAR_MISC & 3) != 3))
      MSIX_irq(rVTIVAR_MISC & 0x3);
  }

  void RX_irq(unsigned nr)
  {
    uint32 va = rVTIVAR >> (nr*16);
    if ((va & 0x80) != 0)
      MSIX_irq(va & 0x3);
  }

  void TX_irq(unsigned nr)
  {
    uint32 va = rVTIVAR >> (nr*16 + 8);
    if ((va & 0x80) != 0)
      MSIX_irq(va & 0x3);
  }

  void VTEITR_cb(uint32 old, uint32 val)
  {
    // Do nothing. Not implemented.
  }

  void VMMB_cb(uint32 old, uint32 val)
  {
    // See 82576 datasheet Table 7-71 on page 357 for a good
    // explanation how this is supposed to work.

    // XXX Handle writes to VFU properly.
    if ((val & 1) != 0) {
      // Request for PF
      switch (rVFMBX0 & 0xFFFF) {
      case VF_RESET:
	rVFMBX0 |= CMD_ACK;
	rVFMBX1 = _mac.raw;
	rVFMBX2 = (_mac.raw >> 32) & 0xFFFF;
	Logging::printf("VF_RESET " MAC_FMT "\n", MAC_SPLIT(&_mac));
	break;
      case VF_SET_MAC_ADDR:
	rVFMBX0 |= CMD_ACK;
	_mac.raw = static_cast<uint64>(rVFMBX2 & 0xFFFF) << 32 | rVFMBX1;
	Logging::printf("VF_SET_MAC " MAC_FMT "\n", MAC_SPLIT(&_mac));
	break;
      case VF_SET_MULTICAST: {
	uint8 count = (rVFMBX0 >> 16) & 0xFF;
	Logging::printf("VF_SET_MULTICAST %08x (%u) %08x\n",
			rVFMBX0, count, rVFMBX1);

	// Linux never sends more than 30 hashes.
	if (count > 30) count = 30;

	_mta.clear();
	uint16 *hash = reinterpret_cast<uint16 *>(&rVFMBX1);
	for (unsigned i = 0; i < count; i++) {
	  _mta.set(hash[i]);
	}

	rVFMBX0 |= CMD_ACK | CTS;
      }
	break;
      case VF_SET_PROMISC:
	Logging::printf("VF_SET_PROMISC %08x %08x %08x\n", rVFMBX0,
			rVFMBX1, rVFMBX2);
	_promisc = (rVFMBX0 & VF_SET_PROMISC_UNICAST);
	Logging::printf("Promiscuous mode is %s.\n", _promisc ? "ENABLED" : "DISABLED");
	rVFMBX0 |= CMD_ACK | CTS;
	break;
      default:
	Logging::printf("VF message unknown %08x\n", rVFMBX0 & 0xFFFF);
	rVFMBX0 |= CMD_NACK;
	// XXX
	break;
      }
      // Claim the buffer in a magic atomic way. If this were a real
      // VF, this would probably not happen at once.
      rVMMB = (rVMMB & ~(1<<2 /* VFU */)) | (1<<3 /* PFU */);
      // We have ACKed and wrote a response. Send an IRQ to inform the
      // VM.
      rVMMB |= 1<<5 | 1<<4;     /* PFACK | PFSTS */
      MISC_irq();
    } else if ((val & 2) != 0) {
      // VF ACKs our message. Clear PFU to let the VM send a new
      // message.
      rVMMB &= (1<<3 /* PFU */);
    }

    rVMMB &= ~3;
  }

  void VTCTRL_cb(uint32 old, uint32 val)
  {
    if ((old ^ val) & (1<<26 /* Reset */)) {
      MMIO_init();
      // XXX Anything else to do here?
    }
  }

  void VTEICS_cb(uint32 old, uint32 val)
  {
    for (unsigned i = 0; i < 3; i++)
      if ((rVTEICR & (1<<i)) != 0) MSIX_irq(i);
  }

public:

  bool receive(MessagePciConfig &msg)
  {
    if (msg.bdf != _bdf) return false;

    switch (msg.type) {
    case MessagePciConfig::TYPE_READ:
      msg.value = PCI_read(msg.dword<<2);
      //Logging::printf("PCICFG READ  %02x -> %08x\n", msg.dword, msg.value);
      break;
    case MessagePciConfig::TYPE_WRITE:
      //Logging::printf("PCICFG WRITE %02x <- %08x\n", msg.dword, msg.value);
      PCI_write(msg.dword<<2, msg.value);
      break;
    case MessagePciConfig::TYPE_PTR:
      break;
    }
    return true;
  }

  uint32 MSIX_read(uint32 offset)
  {
    if (offset >= sizeof(_msix)) return 0;
    return _msix.raw[offset/4];
  }

  uint32 MSIX_write(uint32 offset, uint32 val)
  {
    if (offset >= sizeof(_msix)) return 0;
    // vector_control has only 1 mutable bit.
    return _msix.raw[offset/4] = val & (((offset & 0xF) == 0xC) ? 1 : ~0U);
  }

  // XXX Clean up!
  bool receive(MessageMem &msg)
  {
    // Memory decode disabled?
    if ((rPCISTSCTRL & 2) == 0) return false;

    if (msg.read) {
      if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
	uint32 offset = msg.phys - (rPCIBAR0 & ~0x3FFF);
	//Logging::printf("MMIO READ  %lx\n", offset);
	switch (offset >> 12) {
	case 2:  *msg.ptr = _rx_queues[(offset & 0x100) ? 1 : 0].read(offset); break;
	case 3:  *msg.ptr = _tx_queues[(offset & 0x100) ? 1 : 0].read(offset); break;
	default: *msg.ptr = MMIO_read(offset); break;
	}
      } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
	*msg.ptr = MSIX_read(msg.phys - (rPCIBAR3 & ~0xFFF));
      } else return false;
      return true;
    }

    if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
      uint32 offset = msg.phys - (rPCIBAR0 & ~0x3FFF);
      //Logging::printf("MMIO WRITE %lx\n", offset);
      switch (offset >> 12) {
      case 2: _rx_queues[(offset & 0x100) ? 1 : 0].write(offset, *msg.ptr); break;
      case 3: _tx_queues[(offset & 0x100) ? 1 : 0].write(offset, *msg.ptr); break;
      default: MMIO_write(msg.phys - (rPCIBAR0 & ~0x3FFF), *msg.ptr); break;
      }
    } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
      MSIX_write(msg.phys - (rPCIBAR3 & ~0xFFF), *msg.ptr);
    } else return false;
    return true;
  }

  bool receive(MessageNetwork &msg)
  {
    // XXX Hack. Avoid our own packets.
    if (!(((msg.buffer < _tx_queues[0].packet_buf) ||
	   (msg.buffer >= (_tx_queues[0].packet_buf + sizeof(_tx_queues[0].packet_buf)))) &&
	  ((msg.buffer < _tx_queues[1].packet_buf) ||
	   (msg.buffer >= (_tx_queues[1].packet_buf + sizeof(_tx_queues[1].packet_buf))))))
      return false;

    _rx_queues[0].receive_packet(const_cast<uint8 *>(msg.buffer), msg.len);
    return true;
  }

  void reprogram_timer()
  {
    assert(_txpoll_us != 0);
    MessageTimer msgn(_timer_nr, _clock->abstime(_txpoll_us, 1000000));
    if (!_timer.send(msgn))
      Logging::panic("%s could not program timer.", __PRETTY_FUNCTION__);
  }

  bool receive(MessageMemRegion &msg)
  {
    switch ((msg.page) - (_mem_mmio >> 12)) {
    case 0x2:
      if (!_map_rx) return false;
      msg.ptr =  reinterpret_cast<char *>(_local_rx_regs);
      msg.start_page = msg.page;
      msg.count = 1;
      break;
    case 0x3:
      if (_txpoll_us != 0) {
	msg.ptr =  reinterpret_cast<char *>(_local_tx_regs);
	msg.start_page = msg.page;
	msg.count = 1;
	// If TX memory is mapped, we need to poll it periodically.
	reprogram_timer();

	break;
      } else {
	// If _txpoll_us is zero, we don't map TX registers and don't
	// need to poll.
	// FALLTHROUGH
      }
    default:
      return false;
    }

    Logging::printf("82576VF MAP %lx+%x from %p\n", msg.page, msg.count, msg.ptr);
    return true;
  }

  bool receive(MessageTimeout &msg)
  {
    if (msg.nr != _timer_nr) return false;

    for (unsigned i = 0; i < 2; i++) {
      _tx_queues[i].txdctl_poll();
      _tx_queues[i].tdt_poll();
    }

    reprogram_timer();
    return true;
  }

  void device_reset()
  {
    PCI_init();
    rPCIBAR0 = _mem_mmio;
    rPCIBAR3 = _mem_msix;

    for (unsigned i = 0; i < 3; i++) {
      _msix.table[i].msg_addr = 0;
      _msix.table[i].msg_data = 0;
      _msix.table[i].vector_control = 1;
    }

    MMIO_init();

    _mta.clear();
    _promisc = _promisc_default;

    for (unsigned i = 0; i < 2; i++) {
      _tx_queues[i].reset();
      _rx_queues[i].reset();
    }
  }

  bool receive(MessageLegacy &msg)
  {
    if (msg.type == MessageLegacy::RESET)
      device_reset();

    return false;
  }

  Model82576vf(uint64 mac, DBus<MessageNetwork> &net,
	       DBus<MessageMem> *bus_mem, DBus<MessageMemRegion> *bus_memregion,
	       Clock *clock, DBus<MessageTimer> &timer,
	       uint32 mem_mmio, uint32 mem_msix, unsigned txpoll_us, bool map_rx, unsigned bdf,
	       bool promisc_default)
    : _mac(mac), _net(net), _bus_memregion(bus_memregion), _bus_mem(bus_mem),
      _clock(clock), _timer(timer),
      _mem_mmio(mem_mmio), _mem_msix(mem_msix),
      _txpoll_us(txpoll_us), _map_rx(map_rx), _bdf(bdf),
      _promisc_default(promisc_default)
  {
    Logging::printf("Attached 82576VF model at %08x+0x4000, %08x+0x1000\n",
		    mem_mmio, mem_msix);

    // Init queues
    _local_rx_regs = new (4096) uint32[1024];
    _rx_queues[0].init(this, 0, _local_rx_regs);
    _rx_queues[1].init(this, 1, _local_rx_regs + 0x100/4);

    _local_tx_regs = new (4096) uint32[1024];
    _tx_queues[0].init(this, 0, _local_tx_regs);
    _tx_queues[1].init(this, 1, _local_tx_regs + 0x100/4);

    device_reset();

    // Program timer
    MessageTimer msgt;
    if (!_timer.send(msgt))
      Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);
    _timer_nr = msgt.nr;
  }

};

PARAM_HANDLER(82576vf,
	      "82576vf:[promisc][,mem_mmio][,mem_msix][,txpoll_us][,rx_map] - attach an Intel 82576VF to the PCI bus.",
	      "promisc   - if !=0, be always promiscuous (use for Linux VMs that need it for bridging) (Default 1)",
	      "txpoll_us - if !=0, map TX registers to guest and poll them every txpoll_us microseconds. (Default 0)",
	      "rx_map    - if !=0, map RX registers to guest. (Default: Yes)",
	      "Example: 82576vf"
	      )
{
  MessageHostOp msg(MessageHostOp::OP_GET_MAC, 0UL);
  if (!mb.bus_hostop.send(msg)) Logging::panic("Could not get a MAC address");

  Model82576vf *dev = new Model82576vf(hton64(msg.mac) >> 16,
				       mb.bus_network, &mb.bus_mem, &mb.bus_memregion,
				       mb.clock(), mb.bus_timer,
				       (argv[1] == ~0UL) ? 0xF7CE0000 : argv[1],
				       (argv[2] == ~0UL) ? 0xF7CC0000 : argv[2],
				       (argv[3] == ~0UL) ? 0 : argv[3],
				       argv[4],
				       PciHelper::find_free_bdf(mb.bus_pcicfg, ~0U),
				       (argv[0] == ~0UL) ? true : (argv[0] != 0) );
  mb.bus_mem.add(dev, &Model82576vf::receive_static<MessageMem>);
  mb.bus_memregion.add(dev, &Model82576vf::receive_static<MessageMemRegion>);
  mb.bus_pcicfg.  add(dev, &Model82576vf::receive_static<MessagePciConfig>);
  mb.bus_network. add(dev, &Model82576vf::receive_static<MessageNetwork>);
  mb.bus_timeout. add(dev, &Model82576vf::receive_static<MessageTimeout>);
  mb.bus_legacy.  add(dev, &Model82576vf::receive_static<MessageLegacy>);
}



// EOF
