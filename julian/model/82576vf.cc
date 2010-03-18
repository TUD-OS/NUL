// -*- Mode: C++ -*-

#include <nul/types.h>
#include <nul/motherboard.h>
#include <service/math.h>
#include <model/pci.h>

/// NOTES
//
// We emulate more bits in the PCI COMMAND register than the real VF
// supports.
//
// We can probably map the receive register range 0x2000 - 0x2FFF
// directly into VM space, if we ignore RXDCTL.SWFLUSH (Software
// Flush).

class Model82576vf : public StaticReceiver<Model82576vf>
{
  uint64 _mac;
  DBus<MessageNetwork>  &_net;
  DBus<MessageIrq>      &_irqlines;
  DBus<MessageMemWrite> &_memwrite;
  DBus<MessageMemRead>  &_memread;

  uint32 _mem_mmio;
  uint32 _mem_msix;

#include <model/82576vfmmio.inc>
#include <model/82576vfpci.inc>

  struct rx_queue {
    Model82576vf &parent;
    unsigned n;

    uint32 &rxdctl;
    uint32 &rdbal;
    uint32 &rdbah;
    uint32 &rdt;
    uint32 &rdh;
    uint32 &rdlen;
    uint32 &srrctl;

    typedef union {
      uint64 raw[2];
      struct {
	uint64 buffer;
	uint32 status;
	uint32 sumlen;
      } legacy;
    } rx_desc ;

    void rxdctl_cb(uint32 old, uint32 val)
    {
      if (((old ^ val) & (1<<25)) != 0) {
	// Enable/Disable receive queue 0
	if ((rxdctl & (1<<25)) != 0) {
	  // Enable queue. Reset registers.
	  Logging::printf("Enabling queue %u.\n", n);
	  rdh = 0;
	  rdt = 0;
	} else {
	  // Disable queue. Keep register content.
	  Logging::printf("Disabling queue %u.\n", n);
	}
      }
    }

    void receive_packet(const uint8 *buf, size_t size)
    {
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
      if (!parent._memread.send(msg)) {
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
	  if (!parent._memwrite.send(m))
	    desc.legacy.status |= 0x8000; // RX error
	  desc.legacy.status |= 0x3; // EOP, DD
	  desc.legacy.sumlen = size;
	}
	break;
      default:
	Logging::printf("Invalid descriptor type %x\n", desc_type);
	break;
      }

      MessageMemWrite m(addr, desc.raw, sizeof(desc));
      if (!parent._memwrite.send(m))
	Logging::printf("RX descriptor store failed.\n");

      // Advance queue head
      rdh = (((rdh+1)*16 ) % rdlen) / 16;

      parent.RX_irq(n);
    }
  } _rx_queues[2];

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
    Logging::printf("MSI-X IRQ %d | EIMS %02x | C %02x\n", nr,
		    rVTEIMS, _msix.table[nr].vector_control);
    uint32 mask = 1<<nr;
    // Set interrupt cause.
    rVTEICR |= mask;

    if ((mask & rVTEIMS) != 0) {
      if ((_msix.table[nr].vector_control & 1) == 0) {
	Logging::printf("Generating MSI-X IRQ %d\n", nr);
	// XXX Proper MSI delivery. ASSERT_IRQ or ASSERT_NOTIFY?
	MessageIrq msg(MessageIrq::ASSERT_IRQ, _msix.table[nr].msg_data & 0xFF);
	_irqlines.send(msg);
      }
      // Auto-Clear
      // XXX Do we auto-clear even if the interrupt cause was masked?
      // The spec is not clear on this.
      rVTEICR &= ~(mask & rVTEIAC);
      rVTEIMS &= ~(mask & rVTEIAM);
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
      rVMMB |= 1<<5 | 1<<4;	/* PFACK | PFSTS */
      MISC_irq();
    } else if ((val & 2) != 0) {
      // VF ACKs our message. Clear PFU to let the VM send a new
      // message.
      rVMMB &= (1<<3 /* PFU */);
    }

    rVMMB &= ~3;
  }

  void RXDCTL0_cb(uint32 old, uint32 val) { _rx_queues[0].rxdctl_cb(old, val); }
  void RXDCTL1_cb(uint32 old, uint32 val) { _rx_queues[1].rxdctl_cb(old, val); }

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
      *(reinterpret_cast<uint32 *>(msg.ptr)) = MMIO_read(msg.phys - (rPCIBAR0 & ~0x3FFF));
    } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
      *(reinterpret_cast<uint32 *>(msg.ptr)) = MSIX_read(msg.phys - (rPCIBAR3 & ~0xFFF));
    } else return false;

    // Logging::printf("PCIREAD %lx (%d) %x \n", msg.phys, msg.count,
    // 		    *reinterpret_cast<unsigned *>(msg.ptr));
    return true;
  }

  bool receive(MessageMemWrite &msg)
  {
    // Memory decode disabled?
    if ((rPCISTSCTRL & 2) == 0) return false;

    // XXX Assert msg.count == 4
    // Logging::printf("PCIWRITE %lx (%d) %x \n", msg.phys, msg.count,
    // 		    *reinterpret_cast<unsigned *>(msg.ptr));
    if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
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

  Model82576vf(uint64 mac, DBus<MessageNetwork> &net, DBus<MessageIrq> &irqlines,
	       DBus<MessageMemWrite> &memwrite, DBus<MessageMemRead> &memread,
	       uint32 mem_mmio, uint32 mem_msix)
    : _mac(mac), _net(net), _irqlines(irqlines),
      _memwrite(memwrite), _memread(memread),
      _mem_mmio(mem_mmio), _mem_msix(mem_msix),
      _rx_queues( {{ *this, 0, rRXDCTL0, rRDBAL0, rRDBAH0, rRDT0, rRDH0, rRDLEN0, rSRRCTL0 },
		   { *this, 1, rRXDCTL1, rRDBAL1, rRDBAH1, rRDT1, rRDH1, rRDLEN1, rSRRCTL1 }})
  {
    Logging::printf("Attached 82576VF model at %08x+0x4000, %08x+0x1000\n",
		    mem_mmio, mem_msix);

    PCI_init();
    rPCIBAR0 = _mem_mmio;
    rPCIBAR3 = _mem_msix;

    for (unsigned i = 0; i < 3; i++) {
      _msix.table[i].msg_addr = 0;
      _msix.table[i].msg_data = 0;
      _msix.table[i].vector_control = 1;
    }

    MMIO_init();
  }

};

PARAM(82576vf,
      {
	MessageHostOp msg(MessageHostOp::OP_GET_UID, ~0);
	if (!mb.bus_hostop.send(msg)) Logging::printf("Could not get an UID");

	Logging::printf("Our UID is %lx\n", msg.value);

	Model82576vf *dev = new Model82576vf(static_cast<uint64>(Math::htonl(msg.value))<<16 | 0xC25000,
					     mb.bus_network, mb.bus_irqlines,
					     mb.bus_memwrite, mb.bus_memread,
					     argv[0], argv[1]);
	mb.bus_memwrite.add(dev, &Model82576vf::receive_static<MessageMemWrite>);
	mb.bus_memread.add(dev, &Model82576vf::receive_static<MessageMemRead>);
	mb.bus_pcicfg.add(dev, &Model82576vf::receive_static<MessagePciConfig>,
			  PciHelper::find_free_bdf(mb.bus_pcicfg, ~0U));
	mb.bus_network.add(dev, &Model82576vf::receive_static<MessageNetwork>);

      },
      "82576vf:mem_mmio,mem_msix - attach an Intel 82576VF to the PCI bus."
      "Example: 82576vf:0xf7ce0000,0xf7cc0000"
      );


// EOF
