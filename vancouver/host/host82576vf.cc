// -*- Mode: C++ -*-
// Host Intel 82576 Virtual Function driver.

#include <vmm/motherboard.h>
#include <host/hostpci.h>
#include <host/host82576.h>

class Host82576VF : public Base82576, public StaticReceiver<Host82576VF>
{
private:

  DBus<MessageHostOp> &_bus_hostop;

  unsigned _hostirqs[3];
  volatile uint32_t *_hwreg;     // Device MMIO registers (128K)

  struct msix_table {
    volatile uint64_t msg_addr;
    volatile uint32_t msg_data;
    volatile uint32_t vector_control;
  } *_msix_table;

  EthernetAddr _mac;

public:

  bool receive(MessageIrq &irq_msg)
  {
    //if (irq_msg.line != _hostirq || irq_msg.type != MessageIrq::ASSERT_IRQ)  return false;
    for (unsigned i = 0; i < 3; i++) {
      if (irq_msg.line == _hostirqs[i]) {
	msg(IRQ, "IRQ%d (%d)!\n", irq_msg.line, i);
	return true;
      }
    }
    return false;
  }

  Host82576VF(HostPci pci, DBus<MessageHostOp> &bus_hostop, Clock *clock,
	      unsigned bdf, unsigned irqs[3],
	      void *reg,
	      void *msix_reg)
    : Base82576(clock, ALL, bdf), _bus_hostop(bus_hostop),
      _hwreg((volatile uint32_t *)reg), _msix_table((struct msix_table *)msix_reg)
  {
    memcpy(_hostirqs, irqs, sizeof(irqs));

    msg(INFO, "Found Intel 82576VF-style controller.\n");

    // Disable IRQs
    _hwreg[VTEIMC] = ~0U;

    // Enable MSI-X
    for (unsigned i = 0; i < 3; i++) {
      _msix_table[i].msg_addr = 0xFEE00000;
      _msix_table[i].msg_data = _hostirqs[i] + 0x20;
      _msix_table[i].vector_control &= ~1;
    }
    pci.conf_write(bdf, pci.find_cap(bdf, pci.CAP_MSIX), MSIX_ENABLE);

    // Setup IRQ mapping:
    // Both RX queues get MSI-X vector 0, both TX queues get 1.
    // Mailbox IRQs go to 2.
    _hwreg[VTIVAR]      = 0x81808180;
    _hwreg[VTIVAR_MISC] = 0x82;

    // Enable IRQs
    _hwreg[VTEIAC] = 3;		// Autoclear for all IRQs
    _hwreg[VTEIAM] = 3;
    _hwreg[VTEIMS] = 3;

    // Send RESET message
    _hwreg[VMB] = VFU;
    _hwreg[VBMEM] = VF_RESET;
    _hwreg[VMB] = Sts;
    
  }

};

PARAM(host82576vf, {
    HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
    uint16_t parent_bdf = argv[0];
    unsigned vf_no      = argv[1];
    uint16_t vf_bdf     = pci.vf_bdf(parent_bdf, vf_no);


    if (pci.vf_device_id(parent_bdf) != 0x10ca8086) {
      Logging::printf("Invalid device.\n");
      return;
    }

    unsigned sriov_cap = pci.find_extended_cap(parent_bdf, pci.EXTCAP_SRIOV);
    uint16_t numvfs = pci.conf_read(parent_bdf, sriov_cap + 0x10) & 0xFFFF;
    if (vf_no >= numvfs) {
      Logging::printf("VF%d does not exist.\n", vf_no);
      return;
    }

    MessageHostOp reg_msg(MessageHostOp::OP_ALLOC_IOMEM,
			  pci.vf_bar_base(parent_bdf, 0) + pci.vf_bar_size(parent_bdf, 0)*vf_no,
			  pci.vf_bar_size(parent_bdf, 0));

    MessageHostOp msix_msg(MessageHostOp::OP_ALLOC_IOMEM,
			   pci.vf_bar_base(parent_bdf, 3) + pci.vf_bar_size(parent_bdf, 3)*vf_no,
			   pci.vf_bar_size(parent_bdf, 3));

    if (!(mb.bus_hostop.send(reg_msg) && mb.bus_hostop.send(msix_msg) &&
	  reg_msg.ptr && msix_msg.ptr)) {
      Logging::printf("Could not map register windows.\n");
      return;
    }

    // Setup DMA remapping
    MessageHostOp assign_msg(MessageHostOp::OP_ASSIGN_PCI, parent_bdf, vf_bdf);
    if (!mb.bus_hostop.send(assign_msg)) {
      Logging::printf("Could not assign PCI device.\n");
      return;
    }

    // IRQs
    unsigned irqs[3];

    for (unsigned i = 0; i < 3; i++) {
      irqs[i] = pci.get_gsi(vf_bdf);
      MessageHostOp irq_msg(MessageHostOp::OP_ATTACH_HOSTIRQ, irqs[i]);
      if (!(irq_msg.value == ~0U || mb.bus_hostop.send(irq_msg)))
	Logging::panic("%s failed to attach hostirq %x\n", __PRETTY_FUNCTION__, irqs[i]);
    }

    Host82576VF *dev = new Host82576VF(pci, mb.bus_hostop, mb.clock(), vf_bdf,
				       irqs, reg_msg.ptr, msix_msg.ptr);

    mb.bus_hostirq.add(dev, &Host82576VF::receive_static<MessageIrq>);
  },
  "host82576vf:parent,vf - provide driver for Intel 82576 virtual function.",
  "Example: 'host82576vf:0x100,0'");

// EOF
