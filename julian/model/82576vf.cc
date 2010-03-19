// -*- Mode: C++ -*-

#include <nul/types.h>
#include <nul/motherboard.h>
#include <service/math.h>
#include <service/time.h>
#include <model/pci.h>

/// NOTES
//
// We emulate more bits in the PCI COMMAND register than the real VF
// supports.
//
// We can probably map the receive register range 0x2000 - 0x2FFF
// directly into VM space, if we ignore RXDCTL.SWFLUSH (Software
// Flush).

// TODO
// - handle resets properly (i.e. reset queues)
// - handle BAR remapping
// - RXDCTL.enable (bit 25) may be racy
// - receive path does not set packet type in RX descriptor


class Model82576vf : public StaticReceiver<Model82576vf>
{
  uint64 _mac;
  DBus<MessageNetwork>  &_net;
  DBus<MessageIrq>      &_irqlines;
  DBus<MessageMemWrite> &_memwrite;
  DBus<MessageMemRead>  &_memread;

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
	Logging::printf("TX: Queue %u not enabled.\n", n);
	return;
      }
      uint32 tdlen = regs[TDLEN];
      if (tdlen == 0) {
	Logging::printf("TX: Queue %u has zero size.\n", n);
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
	MessageMemRead msg(addr, desc.raw, sizeof(desc));
	if (!parent->_memread.send(msg)) {
	  Logging::printf("TX descriptor fetch failed.\n");
	  return;
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
    }

    uint32 read(uint32 offset)
    {
      Logging::printf("TX read %x (%x)\n", offset, (offset & 0x8FF) / 4);
      return regs[(offset & 0x8FF)/4];
    }

    void write(uint32 offset, uint32 val)
    {
      Logging::printf("TX write %x (%x) <- %x\n", offset, (offset & 0x8FF) / 4, val);
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

    void receive_packet(const uint8 *buf, size_t size)
    {
      rxdctl_poll();

      uint32 rdbah  = regs[RDBAH];
      uint32 rdbal  = regs[RDBAL];
      uint32 rdlen  = regs[RDLEN];
      uint32 srrctl = regs[SRRCTL];
      uint32 rdh    = regs[RDH];
      uint32 rdt    = regs[RDT];
      uint32 rxdctl = regs[RXDCTL];

      Logging::printf("RECV %08x %08x %04x %04x\n", rdbal, rdlen, rdt, rdh);
      if ((rxdctl & (1<<25)) == 0) {
      	Logging::printf("Dropped packet: Queue %u not enabled.\n", n);
      	return;
      }
      if (rdlen == 0) {
      	Logging::printf("Dropped packet: Queue %u has zero size.\n", n);
      	return;
      }
      if (rdt == rdh) {
      	Logging::printf("Dropped packet: Queue %u has no receive descriptors available.\n", n);
      	return;
      }

      // assert(rdbah == 0);
      uint64 addr = (static_cast<uint64>(rdbah)<<32 | rdbal) + ((rdh*16) % rdlen);
      rx_desc desc;

      Logging::printf("RX descriptor at %llx\n", addr);
      MessageMemRead msg(addr, desc.raw, sizeof(desc));
      if (!parent->_memread.send(msg)) {
       	Logging::printf("RX descriptor fetch failed.\n");
	return;
      }

      // Which descriptor type?
      uint8 desc_type = (srrctl >> 25) & 0xF;
      switch (desc_type) {
      case 0:			// Legacy
       	{
       	  MessageMemWrite m(desc.legacy.buffer, buf, size);
       	  desc.legacy.status = 0;
       	  if (!parent->_memwrite.send(m))
       	    desc.legacy.status |= 0x8000; // RX error
       	  desc.legacy.status |= 0x3; // EOP, DD
       	  desc.legacy.sumlen = size;
       	}
       	break;
      case 1:			// Advanced, one buffer
	{
	  MessageMemWrite m(desc.advanced_read.pbuffer, buf, size);
	  desc.advanced_write.rss_hash = 0;
	  desc.advanced_write.info = 0;
	  desc.advanced_write.vlan = 0;
	  desc.advanced_write.len = size;
	  desc.advanced_write.status = 0x3; // EOP, DD
       	  if (!parent->_memwrite.send(m))
       	    desc.advanced_write.status |= 0x80000000U; // RX error
	}
	break;
      default:
	 Logging::printf("Invalid descriptor type %x\n", desc_type);
	 break;
      }
      
      MessageMemWrite m(addr, desc.raw, sizeof(desc));
      if (!parent->_memwrite.send(m))
       	Logging::printf("RX descriptor store failed.\n");

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

  // Generate a MSI-X IRQ.
  void MSIX_irq(unsigned nr)
  {
    Logging::printf("MSI-X IRQ %d | EIMS %02x | EIAC %02x | EIAM %02x | C %02x\n", nr,
		    rVTEIMS, rVTEIAC, rVTEIAM, _msix.table[nr].vector_control);
    uint32 mask = 1<<nr;
    // Set interrupt cause.
    rVTEICR |= mask;

    if ((mask & rVTEIMS) != 0) {
      if ((_msix.table[nr].vector_control & 1) == 0) {
	Logging::printf("Generating MSI-X IRQ %d (%02x)\n", nr, _msix.table[nr].msg_data & 0xFF);
	// XXX Proper MSI delivery. ASSERT_IRQ or ASSERT_NOTIFY?
	MessageIrq msg(MessageIrq::ASSERT_IRQ, _msix.table[nr].msg_data & 0xFF);
	_irqlines.send(msg);

	// Auto-Clear
	// XXX Do we auto-clear even if the interrupt cause was masked?
	// The spec is not clear on this.
	rVTEICR &= ~(mask & rVTEIAC);
	rVTEIMS &= ~(mask & rVTEIAM);
	Logging::printf("MSI-X -> EIMS %02x\n", rVTEIMS);
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
    else
      Logging::printf("RX irq %u is disabled.\n", nr);
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

  bool receive(MessageMemRead &msg)
  {
    // Memory decode disabled?
    if ((rPCISTSCTRL & 2) == 0) return false;

    if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
      uint32 offset = msg.phys - (rPCIBAR0 & ~0x3FFF);
      // RX
      if ((offset >> 12) == 0x2) Logging::panic("RX read? Shouldn't happen.");
      // TX
      if ((offset >> 12) == 0x3) 
	*(reinterpret_cast<uint32 *>(msg.ptr)) = _tx_queues[(offset & 0x100) ? 1 : 0].read(offset);
      else
	*(reinterpret_cast<uint32 *>(msg.ptr)) = MMIO_read(offset);
    } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
      *(reinterpret_cast<uint32 *>(msg.ptr)) = MSIX_read(msg.phys - (rPCIBAR3 & ~0xFFF));
    } else return false;

    return true;
  }

  bool receive(MessageMemWrite &msg)
  {
    // Memory decode disabled?
    if ((rPCISTSCTRL & 2) == 0) return false;

    // XXX Assert msg.count == 4
    // Logging::printf("PCIWRITE %lx (%d) %x \n", msg.phys, msg.count,
    //              *reinterpret_cast<unsigned *>(msg.ptr));
    if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
      uint32 offset = msg.phys - (rPCIBAR0 & ~0x3FFF);
      // RX
      if ((offset >> 12) == 0x2) Logging::panic("RX write? Shouldn't happen.");
      // TX
      if ((offset >> 12) == 0x3) 
	_tx_queues[(offset & 0x100) ? 1 : 0].write(offset, *(reinterpret_cast<uint32 *>(msg.ptr)));
      else
	MMIO_write(msg.phys - (rPCIBAR0 & ~0x3FFF), *(reinterpret_cast<uint32 *>(msg.ptr)));
    } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
      MSIX_write(msg.phys - (rPCIBAR3 & ~0xFFF), *(reinterpret_cast<uint32 *>(msg.ptr)));
    } else return false;

    return true;
  }

  bool receive(MessageNetwork &msg)
  {
    // We just receive all.
    // XXX Check if we just sent this.
    _rx_queues[0].receive_packet(msg.buffer, msg.len);
    return true;
  }

  void reprogram_timer()
  {
    assert(_txpoll_us != 0);
    MessageTimer msgn(_timer_nr, _clock->abstime(_txpoll_us, 1000000));
    if (!_timer.send(msgn))
      Logging::panic("%s could not program timer.", __PRETTY_FUNCTION__);
  }

  bool receive(MessageMemMap &msg)
  {
    switch ((msg.phys & ~0xFFF) - _mem_mmio) {
    case 0x2000:
      msg.ptr = _local_rx_regs;
      msg.count = 0x1000;
      break;
    case 0x3000:
      if (_txpoll_us != 0) {
	msg.ptr = _local_tx_regs;
	msg.count = 0x1000;
	
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

    Logging::printf("82576VF MAP %lx+%x from %p\n", msg.phys, msg.count, msg.ptr);
    return true;
  }

  bool receive(MessageTimeout &msg)
  {
    if (msg.nr != _timer_nr) return false;

    for (unsigned i = 1; i < 2; i++) {
      _tx_queues[i].txdctl_poll();
      _tx_queues[i].tdt_poll();
    }

    reprogram_timer();
    return true;
  }
    

  Model82576vf(uint64 mac, DBus<MessageNetwork> &net, DBus<MessageIrq> &irqlines,
	       DBus<MessageMemWrite> &memwrite, DBus<MessageMemRead> &memread,
	       Clock *clock, DBus<MessageTimer> &timer,
	       uint32 mem_mmio, uint32 mem_msix, unsigned txpoll_us)
    : _mac(mac), _net(net), _irqlines(irqlines),
      _memwrite(memwrite), _memread(memread),
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

	Model82576vf *dev = new Model82576vf( //static_cast<uint64>(Math::htonl(msg.value))<<16 | 0xC25000,
					     0xe8e24d211b00ULL,
					     mb.bus_network, mb.bus_irqlines,
					     mb.bus_memwrite, mb.bus_memread,
					     mb.clock(), mb.bus_timer,
					     argv[0], argv[1], (argv[2] == ~0U) ? 1000000 : argv[2] );
	mb.bus_memwrite.add(dev, &Model82576vf::receive_static<MessageMemWrite>);
	mb.bus_memread. add(dev, &Model82576vf::receive_static<MessageMemRead>);
	mb.bus_memmap.  add(dev, &Model82576vf::receive_static<MessageMemMap>);
	mb.bus_pcicfg.  add(dev, &Model82576vf::receive_static<MessagePciConfig>,
			    PciHelper::find_free_bdf(mb.bus_pcicfg, ~0U));
	mb.bus_network. add(dev, &Model82576vf::receive_static<MessageNetwork>);
	mb.bus_timeout. add(dev, &Model82576vf::receive_static<MessageTimeout>);

      },
      "82576vf:mem_mmio,mem_msix[,txpoll_us] - attach an Intel 82576VF to the PCI bus."
      "Example: 82576vf:0xf7ce0000,0xf7cc0000"
      );


// EOF
