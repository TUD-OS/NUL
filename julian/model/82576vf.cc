// -*- Mode: C++ -*-

#include <nul/types.h>
#include <nul/motherboard.h>
#include <service/math.h>
#include <service/time.h>
#include <model/pci.h>

// Status: INCOMPLETE
// Offloads are missing, i.e. context descriptor handling.
// RX path should be fine.
// 
// The model's MAC should match the host driver's MAC. Otherwise you
// might run into trouble. See XXX Change me below.
// 
// What works:
// - udhcpc
// - nc -u -l
// - nc -l
// 
// What doesn't:
// - DNS queries    (UDP segmentation offload?)
// - echo foo | nc  (TCP segmentation offload?)

// This model supports two modes of operation for the TX path:
//  - trap&emulate mode (default):
//     trap every access to TX registers
//  - polled mode:
//     check every n µs for queued packets. n is configured using
//     the txpoll_us parameter (see the comment at the bottom of
//     this file).

// TODO
// - handle resets properly (i.e. reset queues)
// - handle BAR remapping
// - RXDCTL.enable (bit 25) may be racy
// - receive path does not set packet type in RX descriptor
// - TX legacy descriptors
// - TX context descriptors
// - interrupt thresholds


class Model82576vf : public StaticReceiver<Model82576vf>
{
  uint64 _mac;
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

#include <model/82576vfmmio.inc>
#include <model/82576vfpci.inc>

  struct queue {
    Model82576vf *parent;
    unsigned n;
    volatile uint32 *regs;

    virtual void reset();

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

    typedef union {
      uint64 raw[2];
      struct {
	uint64 buffer;
	uint16 dtalen;
	uint8  dtypmacrsv;
	uint8  dcmd;
	uint32 pay;
      } advanced;
    } tx_desc;

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

    // XXX Support huge packets
    uint8 packet_buf[1500];
    unsigned packet_cur;
    
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

    void tdt_poll()
    {
      if ((regs[TXDCTL] & (1<<25)) == 0) {
	if (n == 0) Logging::printf("TX: Queue %u not enabled.\n", n);
	return;
      }
      uint32 tdlen = regs[TDLEN];
      if (tdlen == 0) {
	if (n == 0) Logging::printf("TX: Queue %u has zero size.\n", n);
	return;
      }

      uint32 tdbah = regs[TDBAH];
      uint32 tdbal = regs[TDBAL];

      // Packet send loop.
      uint32 tdh;
      while ((tdh = regs[TDH]) != regs[TDT]) {
	uint64 addr = (static_cast<uint64>(tdbah)<<32 | tdbal) + ((tdh*16) % tdlen);
	tx_desc desc;

	Logging::printf("TX descriptor at %llx\n", addr);
	if (!parent->copy_in(addr, desc.raw, sizeof(desc)))
	  return;
	if ((desc.raw[1] & (1<<29)) == 0) {
	  Logging::printf("TX legacy descriptor: XXX\n");
	  // XXX Not implemented!
	} else {
	  uint8 dtyp = (desc.raw[1] >> 20) & 0xF;
	  switch (dtyp) {
	  case 2:
	    Logging::printf("TX advanced context descriptor XXX\n");
	    Logging::printf("raw[0] = %016llx\n", desc.raw[0]);
	    Logging::printf("raw[1] = %016llx\n", desc.raw[1]);
	    // ping: (dns? -> udp?)
	    // raw[0] = 0000000000001c14
	    // raw[1] = 0000000020200400
	    // netcat (tcp)
	    // raw[0] = 0000000000001c14
	    // raw[1] = 0000000020200c00
	    // XXX Not implemented!
	    break;
	  case 3:
	    {
	      uint8 context = (desc.advanced.pay >> 4) & 0x7;
	      uint32 payload_len = desc.advanced.pay >> 14;
	      uint32 data_len = desc.advanced.dtalen;
	      uint8 dcmd = desc.advanced.dcmd;
	      Logging::printf("TX advanced data descriptor: dcmd %x dta %x pay %x ctx %x\n",
			      dcmd, data_len, payload_len, context);
	      if ((dcmd & (1<<5)) == 0)
		Logging::printf("TX bad descriptor\n");

	      enum {
		EOP = 1,
		IFCS = 2,
		RS = 8,
		VLE = 64,
		TSE = 128,
	      };

	      Logging::printf("%s%s%s%s%s\n", (dcmd&EOP)?"EOP ":"", (dcmd&IFCS)?"IFCS ":"", (dcmd&RS)?"RS ":"",
			      (dcmd&VLE)?"VLE ":"", (dcmd&TSE)?"TSE":"");
	      const uint8 *data = reinterpret_cast<uint8 *>(parent->guestmem(desc.advanced.buffer));

	      if (dcmd&TSE) {
		Logging::printf("XXX We don't support TCP Segmentation Offload.\n");
		goto done;
	      }
	      if ((dcmd&IFCS) == 0) Logging::printf("IFCS not set, but we append FCS anyway in host82576vf.\n");

	      if ((dcmd & EOP) && (payload_len == data_len)) {
		// "Fast path": Complete packet, send as-is.
		MessageNetwork m(data, payload_len, 23); // XXX Set client ID to 23 to avoid our own packets on the RX paths
		parent->_net.send(m);
		packet_cur = 0;
		goto done;
	      }

	      memcpy(packet_buf + packet_cur, data, data_len);
	      packet_cur += data_len;

	      if (dcmd & EOP) {
		MessageNetwork m(data, payload_len, 23); // XXX Set client ID to 23 to avoid our own packets on the RX paths
		parent->_net.send(m);
		packet_cur = 0;
	      }

	    done:
	      // Descriptor is done
	      desc.advanced.pay |= 1;
	      parent->copy_out(addr, desc.raw, sizeof(desc));
	      if ((dcmd & (1<<3) /* Report Status */) != 0)
		parent->TX_irq(n);
	    }
	    break;
	  default:
	    Logging::printf("TX unknown descriptor?\n");
	  }
	}
	  
	// Advance queue head
	asm ( "" ::: "memory");
	regs[TDH] = (((tdh+1)*16 ) % tdlen) / 16;
      }

    }
 

    void reset()
    {
      memset(const_cast<uint32 *>(regs), 0, 0x100);
      regs[TXDCTL] = (n == 0) ? (1<<24) : 0;
      txdctl_old = regs[TXDCTL];
      packet_cur = 0;
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

    struct {
      
    } context[8];

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
      regs[RXDCTL] = 1<<16 | ((n == 0) ? (1<<24) : 0);
      regs[SRRCTL] = 0x400 | ((n != 0) ? 0x80000000U : 0);
      rxdctl_old = regs[RXDCTL];
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
      rxdctl_poll();

      uint32 rdbah  = regs[RDBAH];
      uint32 rdbal  = regs[RDBAL];
      uint32 rdlen  = regs[RDLEN];
      uint32 srrctl = regs[SRRCTL];
      uint32 rdh    = regs[RDH];
      uint32 rdt    = regs[RDT];
      uint32 rxdctl = regs[RXDCTL];

      //Logging::printf("RECV %08x %08x %04x %04x\n", rdbal, rdlen, rdt, rdh);
      if (((rxdctl & (1<<25)) == 0 /* Queue disabled? */)
	  || (rdlen == 0) || (rdt == rdh)) {
	// Drop
      	return;
      }

      // assert(rdbah == 0);
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
       	  desc.legacy.status |= 0x3; // EOP, DD
       	  desc.legacy.sumlen = size;
       	}
       	break;
      case 1:			// Advanced, one buffer
	{
	  desc.advanced_write.rss_hash = 0;
	  desc.advanced_write.info = 0;
	  desc.advanced_write.vlan = 0;
	  desc.advanced_write.len = size;
	  desc.advanced_write.status = 0x3; // EOP, DD
	  if (!parent->copy_out(desc.advanced_read.pbuffer, buf, size))
       	    desc.advanced_write.status |= 0x80000000U; // RX error
	}
	break;
      default:
	 Logging::printf("Invalid descriptor type %x\n", desc_type);
	 break;
      }
      
      if (!parent->copy_out(addr, desc.raw, sizeof(desc)))
	Logging::printf("RX descriptor writeback failed.\n");

      // Advance queue head
      asm ( "" ::: "memory");
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

    PF_CONTROL_MSG   = 0x0100U,

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
	rVFMBX1 = _mac;
	rVFMBX2 = (_mac >> 32) & 0xFFFF;
	break;
      case VF_SET_MAC_ADDR:
	rVFMBX0 |= CMD_ACK;
	_mac = static_cast<uint64>(rVFMBX1) | static_cast<uint64>((rVFMBX2 & 0xFFFF))<<32;
	break;
      case VF_SET_MULTICAST:
	// Ignore
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
    if (msg.bdf) return false;

    switch (msg.type) {
    case MessagePciConfig::TYPE_READ:
      msg.value = PCI_read(msg.dword<<2);
      //Logging::printf("PCICFG READ  %02x -> %08x\n", msg.dword, msg.value);
      break;
    case MessagePciConfig::TYPE_WRITE:
      //Logging::printf("PCICFG WRITE %02x <- %08x\n", msg.dword, msg.value);
      PCI_write(msg.dword<<2, msg.value);
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

  bool receive(MessageMem &msg)
  {
    // Memory decode disabled?
    if ((rPCISTSCTRL & 2) == 0) return false;

    if (msg.read) {
      if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
	uint32 offset = msg.phys - (rPCIBAR0 & ~0x3FFF);
	// RX
	if ((offset >> 12) == 0x2) Logging::panic("RX read? Shouldn't happen.");
	// TX
	if ((offset >> 12) == 0x3)
	  *msg.ptr = _tx_queues[(offset & 0x100) ? 1 : 0].read(offset);
	else
	  *msg.ptr = MMIO_read(offset);
      } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
	*msg.ptr = MSIX_read(msg.phys - (rPCIBAR3 & ~0xFFF));
      } else return false;
      return true;
    }
    // Logging::printf("PCIWRITE %lx (%d) %x \n", msg.phys, msg.count,
    //              *reinterpret_cast<unsigned *>(msg.ptr));
    if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
      uint32 offset = msg.phys - (rPCIBAR0 & ~0x3FFF);
      // RX
      if ((offset >> 12) == 0x2) Logging::panic("RX write? Shouldn't happen.");
      // TX
      if ((offset >> 12) == 0x3)
	_tx_queues[(offset & 0x100) ? 1 : 0].write(offset, *msg.ptr);
      else
	MMIO_write(msg.phys - (rPCIBAR0 & ~0x3FFF), *msg.ptr);
    } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
      MSIX_write(msg.phys - (rPCIBAR3 & ~0xFFF), *msg.ptr);
    } else return false;
    return true;
  }

  bool receive(MessageNetwork &msg)
  {
    // XXX Hack. Avoid our own packets.
    if (msg.client == 23) return false;

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
    

  Model82576vf(uint64 mac, DBus<MessageNetwork> &net,
	       DBus<MessageMem> *bus_mem, DBus<MessageMemRegion> *bus_memregion,
	       Clock *clock, DBus<MessageTimer> &timer,
	       uint32 mem_mmio, uint32 mem_msix, unsigned txpoll_us)
    : _mac(mac), _net(net), _bus_memregion(bus_memregion), _bus_mem(bus_mem),
      _clock(clock), _timer(timer),
      _mem_mmio(mem_mmio), _mem_msix(mem_msix),
      _txpoll_us(txpoll_us)
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

    PCI_init();
    rPCIBAR0 = _mem_mmio;
    rPCIBAR3 = _mem_msix;

    for (unsigned i = 0; i < 3; i++) {
      _msix.table[i].msg_addr = 0;
      _msix.table[i].msg_data = 0;
      _msix.table[i].vector_control = 1;
    }

    MMIO_init();

    // Program timer
    MessageTimer msgt;
    if (!_timer.send(msgt))
      Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);
    _timer_nr = msgt.nr;
  }

};

PARAM(82576vf,
      {
	MessageHostOp msg(MessageHostOp::OP_GET_UID, ~0);
	if (!mb.bus_hostop.send(msg)) Logging::printf("Could not get an UID");

	Logging::printf("Our UID is %lx\n", msg.value);

	Model82576vf *dev = new Model82576vf( // XXX Change me 
					     //0xe8e24d211b00ULL,
					     static_cast<uint64>(Math::htonl(msg.value))<<16 | 0xC25000,

					     mb.bus_network, &mb.bus_mem, &mb.bus_memregion,
					     mb.clock(), mb.bus_timer,
					     argv[0], argv[1], (argv[2] == ~0U) ? 0 : argv[2] );
	mb.bus_mem.add(dev, &Model82576vf::receive_static<MessageMem>);
	mb.bus_memregion.add(dev, &Model82576vf::receive_static<MessageMemRegion>);
	mb.bus_pcicfg.  add(dev, &Model82576vf::receive_static<MessagePciConfig>,
			    PciHelper::find_free_bdf(mb.bus_pcicfg, ~0U));
	mb.bus_network. add(dev, &Model82576vf::receive_static<MessageNetwork>);
	mb.bus_timeout. add(dev, &Model82576vf::receive_static<MessageTimeout>);

      },
      "82576vf:mem_mmio,mem_msix[,txpoll_us] - attach an Intel 82576VF to the PCI bus."
      "Example: 82576vf:0xf7ce0000,0xf7cc0000"
      );


// EOF
