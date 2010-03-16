// -*- Mode: C++ -*-

#include <nul/types.h>
#include <nul/motherboard.h>
#include <service/math.h>
#include <model/pci.h>

// Notes:
// We emulate more bits in the PCI COMMAND register than the real VF supports.

class Model82576vf : public StaticReceiver<Model82576vf>
{
  uint64 _mac;
  DBus<MessageIrq> _irqlines;
  uint32 _mem_mmio;
  uint32 _mem_msix;

#include <model/82576vfmmio.inc>
#include <model/82576vfpci.inc>

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
    uint32 mask = 1<<nr;
    // Set interrupt cause.
    rVTEICR |= mask;

    if ((mask & rVTEIMS) != 0) {
      if ((_msix.table[nr].vector_control & 1) == 0) {
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
    Logging::printf("MISC_irq: IVAR_MISC %x\n", rVTIVAR_MISC);
    if ((rVTIVAR_MISC & 0x80) && ((rVTIVAR_MISC & 3) != 3))
      MSIX_irq(rVTIVAR_MISC & 0x3);
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

  void VTCTRL_cb(uint32 old, uint32 val)
  {
    if ((old ^ val) & (1<<26 /* Reset */)) {
      // XXX Do reset here.
      
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
      Logging::printf("PCICFG READ  %02x -> %08x\n", msg.dword, msg.value);
      break;
    case MessagePciConfig::TYPE_WRITE:
      Logging::printf("PCICFG WRITE %02x <- %08x\n", msg.dword, msg.value);
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

    Logging::printf("PCIREAD %lx (%d) %x \n", msg.phys, msg.count,
		    *reinterpret_cast<unsigned *>(msg.ptr));
    return true;
  }

  bool receive(MessageMemWrite &msg)
  {
    // Memory decode disabled?
    if ((rPCISTSCTRL & 2) == 0) return false;

    // XXX Assert msg.count == 4
    Logging::printf("PCIWRITE %lx (%d) %x \n", msg.phys, msg.count,
		    *reinterpret_cast<unsigned *>(msg.ptr));
    if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
      MMIO_write(msg.phys - (rPCIBAR0 & ~0x3FFF), *(reinterpret_cast<uint32 *>(msg.ptr)));
    } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
      MSIX_write(msg.phys - (rPCIBAR3 & ~0xFFF), *(reinterpret_cast<uint32 *>(msg.ptr)));
    } else return false;
    
    return true;
  }

  void device_reset()
  {
    Logging::printf("82576vf device reset.\n");
    PCI_init();
    MMIO_init();
    
    rPCIBAR0 = _mem_mmio;
    rPCIBAR3 = _mem_msix;

    for (unsigned i = 0; i < 3; i++) {
      _msix.table[i].msg_addr = 0;
      _msix.table[i].msg_data = 0;
      _msix.table[i].vector_control = 1;
    }
  }

  Model82576vf(uint64 mac, DBus<MessageIrq> irqlines, uint32 mem_mmio, uint32 mem_msix) 
    : _mac(mac), _irqlines(irqlines), _mem_mmio(mem_mmio), _mem_msix(mem_msix)
  {
    Logging::printf("Attached 82576VF model at %08x+0x4000, %08x+0x1000\n",
		    mem_mmio, mem_msix);
    device_reset();
  }

};

PARAM(82576vf,
      {
	MessageHostOp msg(MessageHostOp::OP_GET_UID, ~0);
	if (!mb.bus_hostop.send(msg)) Logging::printf("Could not get an UID");

	Logging::printf("Our UID is %lx\n", msg.value);

	Model82576vf *dev = new Model82576vf(static_cast<uint64>(Math::htonl(msg.value))<<16 | 0xC25000,
					     mb.bus_irqlines, argv[0], argv[1]);
	mb.bus_memwrite.add(dev, &Model82576vf::receive_static<MessageMemWrite>);
	mb.bus_memread.add(dev, &Model82576vf::receive_static<MessageMemRead>);
	mb.bus_pcicfg.add(dev, &Model82576vf::receive_static<MessagePciConfig>,
			  PciHelper::find_free_bdf(mb.bus_pcicfg, ~0U));

      },
      "82576vf:mem_mmio,mem_msix - attach an Intel 82576VF to the PCI bus."
      "Example: 82576vf:0xf7ce0000,0xf7cc0000"
      );
      

// EOF
