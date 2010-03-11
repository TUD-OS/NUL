// -*- Mode: C++ -*-

#include <cstdint>
#include <nul/motherboard.h>
#include <model/pci.h>

class Model82576vf : public StaticReceiver<Model82576vf>
{
  DBus<MessageIrq> _irqlines;
  uint32_t _mem_mmio;
  uint32_t _mem_msix;

#include <model/82576vfmmio.inc>
#include <model/82576vfpci.inc>

  struct msix_table {
    uint64_t msg_addr;
    uint32_t msg_data;
    uint32_t vector_control;
  } _msix_table[3];

  uint32_t VTFRTIMER_compute()
  {
    // XXX
    return 0;
  }

  // Generate a MSI-X IRQ.
  void MSIX_irq(unsigned nr)
  {
    if ((_msix_table[nr].vector_control & 1) == 0) {
      // XXX Proper MSI delivery. ASSERT_IRQ or ASSERT_NOTIFY?
      MessageIrq msg(MessageIrq::ASSERT_IRQ, _msix_table[nr].msg_data & 0xFF);
      _irqlines.send(msg);
    }
  }

  /// Generate a mailbox/misc IRQ.
  void MISC_irq()
  {
    if ((rVTIVAR_MISC & 0x80) && ((rVTIVAR_MISC & 3) != 3))
      MSIX_irq(rVTIVAR_MISC & 0x3);
  }

  void VMMB_cb(uint32_t old, uint32_t val)
  {
    if ((rVMMB & 1) != 0) {
      // Request for PF
      // XXX Do something with the message. Write answer and set Sts.
      rVMMB |= 1<<5;		/* PFACK */
      MISC_irq();
    } else if ((rVMMB & 2) != 0) {
      // PF message received. Ignore!
    }
     
    rVMMB &= ~3;
  }

  void VTCTRL_cb(uint32_t old, uint32_t val)
  {
    if ((old ^ val) & (1<<26 /* Reset */)) {
      // XXX Do reset here.
      
    }
  }

  void VTEICR_cb(uint32_t old, uint32_t val)
  {
    // XXX
  }

  void PCI_BAR0_cb(uint32_t old, uint32_t val)
  {
    // XXX
  }

  void PCI_BAR3_cb(uint32_t old, uint32_t val)
  {
    // XXX
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

  uint32_t MSIX_read(uint32_t offset)
  {
    if (offset >= sizeof(_msix_table)) return 0;
    return ((uint32_t *)_msix_table)[offset/4];
  }

  uint32_t MSIX_write(uint32_t offset, uint32_t val)
  {
    if (offset >= sizeof(_msix_table)) return 0;
    // vector_control has only 1 mutable bit.
    return ((uint32_t *)_msix_table)[offset/4] = val & (((offset & 0xF) == 0xC) ? 1 : ~0U);
  }

  bool receive(MessageMemRead &msg)
  {
    // Memory decode disabled?
    if ((rPCISTSCTRL & 2) == 0) return false;

    if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
      *(reinterpret_cast<uint32_t *>(msg.ptr)) = MMIO_read(msg.phys - (rPCIBAR0 & ~0x3FFF));
    } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
      *(reinterpret_cast<uint32_t *>(msg.ptr)) = MSIX_read(msg.phys - (rPCIBAR3 & ~0xFFF));
    } else return false;

    Logging::printf("PCIREAD %lx (%d) %x \n", msg.phys, msg.count, *(unsigned *)msg.ptr);
    return true;
  }

  bool receive(MessageMemWrite &msg)
  {
    // Memory decode disabled?
    if ((rPCISTSCTRL & 2) == 0) return false;

    // XXX Assert msg.count == 4
    Logging::printf("PCIWRITE %lx (%d) %x \n", msg.phys, msg.count, *(unsigned *)msg.ptr);
    if ((msg.phys & ~0x3FFF) == (rPCIBAR0 & ~0x3FFF)) {
      MMIO_write(msg.phys - (rPCIBAR0 & ~0x3FFF), *(reinterpret_cast<uint32_t *>(msg.ptr)));
    } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
      MSIX_write(msg.phys - (rPCIBAR3 & ~0xFFF), *(reinterpret_cast<uint32_t *>(msg.ptr)));
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
      _msix_table[i].msg_addr = 0;
      _msix_table[i].msg_data = 0;
      _msix_table[i].vector_control = 1;
    }
  }

  Model82576vf(DBus<MessageIrq> irqlines, uint32_t mem_mmio, uint32_t mem_msix) 
    : _irqlines(irqlines), _mem_mmio(mem_mmio), _mem_msix(mem_msix)
  {
    Logging::printf("Attached 82576VF model at %08x+0x4000, %08x+0x1000\n",
		    mem_mmio, mem_msix);
    device_reset();
  }

};

PARAM(82576vf,
      {
	Model82576vf *dev = new Model82576vf(mb.bus_irqlines, argv[0], argv[1]);
	mb.bus_memwrite.add(dev, &Model82576vf::receive_static<MessageMemWrite>);
	mb.bus_memread.add(dev, &Model82576vf::receive_static<MessageMemRead>);
	mb.bus_pcicfg.add(dev, &Model82576vf::receive_static<MessagePciConfig>,
			  PciHelper::find_free_bdf(mb.bus_pcicfg, ~0U));

      },
      "82576vf:mem_mmio,mem_msix - attach an Intel 82576VF to the PCI bus."
      "Example: 82576vf:f7ce0000,f7cc0000"
      );
      

// EOF
