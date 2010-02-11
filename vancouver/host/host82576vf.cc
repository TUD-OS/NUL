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
  volatile uint32_t *_hwreg;     // Device MMIO registers (16K)

  struct msix_table {
    volatile uint64_t msg_addr;
    volatile uint32_t msg_data;
    volatile uint32_t vector_control;
  } *_msix_table;

  EthernetAddr _mac;
  
  bool _up;			// Are we UP?

public:

  void reset_complete()
  {
    if (_up)
      msg(INFO, "Another reset?!\n");

    _up = true;
    _hwreg[RXDCTL0] |= (1<<25);
  }

  bool receive(MessageIrq &irq_msg)
  {
    for (unsigned i = 0; i < 3; i++) {
      if (irq_msg.line == _hostirqs[i]) {
	unsigned eicr = _hwreg[EICR];
	_hwreg[EICR] = eicr;

	//msg(IRQ, "IRQ%d (%d) EICR %x\n", irq_msg.line, i, eicr);

	if (eicr & 1) {
	  // RX IRQ
	  msg(IRQ, "RX\n");
	}

	if (eicr & 2) {
	  // TX IRQ
	  msg(IRQ, "TX\n");
	}

	if (eicr & 4) {
	  // MBX IRQ: Just handle RESET for now. Seems like we don't
	  // need more right now.
	  uint32_t vmb = _hwreg[VMB];

	  if (vmb & (1<<4 /* PFSTS */)) {
	    // Claim message buffer
	    _hwreg[VMB] = 1<<2 /* VFU */;
	    if ((_hwreg[VMB] & (1<<2)) == 0) {
	      msg(INFO, "MBX Could not claim buffer.\n");
	      goto mbx_done;
	    }
	    uint32_t msg0 = _hwreg[VBMEM];
	    switch (msg0 & (0xFF|CMD_ACK|CMD_NACK)) {
	    case (VF_RESET | CMD_ACK):
	      _mac.raw = _hwreg[VBMEM + 1];
	      _mac.raw |= (((uint64_t)_hwreg[VBMEM + 2]) & 0xFFFFULL) << 32;
	      _hwreg[VMB] = 1<<1 /* ACK */;
	      msg(VF, "We are " MAC_FMT "\n", MAC_SPLIT(&_mac));
	      reset_complete();
	      break;
	    default:
	      msg(IRQ, "Unrecognized message.\n");
	      _hwreg[VMB] = 1<<1 /* ACK */;
	    }
	  }
	}
      mbx_done:
	
	_hwreg[EIMS] = eicr;
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
      _hwreg((volatile uint32_t *)reg), _msix_table((struct msix_table *)msix_reg), _up(false)
  {
    memcpy(_hostirqs, irqs, sizeof(_hostirqs));

    msg(INFO, "Found Intel 82576VF-style controller.\n");

    // Disable IRQs
    _hwreg[VTEIMC] = ~0U;
    _hwreg[VTCTRL] |= CTRL_RST;
    _hwreg[VTEIMC] = ~0U;

    // Enable MSI-X
    for (unsigned i = 0; i < 3; i++) {
      _msix_table[i].msg_addr = 0xFEE00000;
      _msix_table[i].msg_data = _hostirqs[i] + 0x20;
      _msix_table[i].vector_control &= ~1;
    }
    pci.conf_write(bdf, pci.find_cap(bdf, pci.CAP_MSIX), MSIX_ENABLE);
    pci.conf_write(bdf, 0x4 /* CMD */, 1U<<2 /* Bus-Master enable */);

    // Setup IRQ mapping:
    // RX queue 0 get MSI-X vector 0, TX queue 0 gets 1.
    // Mailbox IRQs go to 2.
    _hwreg[VTIVAR]      = 0x00008180;
    _hwreg[VTIVAR_MISC] = 0x82;

    // Enable IRQs
    _hwreg[VTEIAC] = 7;		// Autoclear for all IRQs
    _hwreg[VTEIAM] = 7;
    _hwreg[VTEIMS] = 7;

    // Send RESET message
    _hwreg[VMB] = VFU;
    _hwreg[VBMEM] = VF_RESET;
    _hwreg[VMB] = Sts;

    // Enable RX
    //_hwreg[RXDCTL0] |= (1U<<25);
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
