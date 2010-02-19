// -*- Mode: C++ -*-
// Host Intel 82576 Virtual Function driver.

#include <vmm/motherboard.h>
#include <host/hostpci.h>
#include <host/host82576.h>

static const unsigned desc_ring_len = 32;

typedef uint8_t packet_buffer[2048];

struct dma_desc {
  uint64_t lo;
  uint64_t hi;
};

class Host82576VF : public Base82576, public StaticReceiver<Host82576VF>
{
private:

  DBus<MessageHostOp> &_bus_hostop;
  DBus<MessageNetwork> &_bus_network;

  struct {
    unsigned vec;
    void (Host82576VF::*handle)();
  } _hostirqs[3];

  volatile uint32_t *_hwreg;     // Device MMIO registers (16K)

  struct msix_table {
    volatile uint64_t msg_addr;
    volatile uint32_t msg_data;
    volatile uint32_t vector_control;
  } *_msix_table;

  EthernetAddr _mac;
  
  bool _up;			// Are we UP?

  unsigned last_rx;
  dma_desc _rx_ring[desc_ring_len] __attribute__((aligned(128)));

  unsigned last_tx;
  dma_desc _tx_ring[desc_ring_len] __attribute__((aligned(128)));

  packet_buffer _rx_buf[desc_ring_len];
  packet_buffer _tx_buf[desc_ring_len];

public:

  void reset_complete()
  {
    if (_up)
      msg(INFO, "Another reset?!\n");

    _up = true;
    _hwreg[RXDCTL0] |= (1<<25);
  }

  void handle_rx()
  {
    unsigned handle = desc_ring_len/2;
    dma_desc *cur;
    while ((cur = &_rx_ring[last_rx])->hi & 1 /* done? */) {
      uint16_t plen = cur->hi >> 32;
      msg(INFO, "RX %02x! %016llx %016llx (len %04x)\n", last_rx, cur->lo, cur->hi, plen);
      if (handle-- == 0) {
	msg(INFO, "Too many packets. Exit handle_rx for now.\n");
	_hwreg[VTEICS] = 1;	// XXX Needed?
	return;
      }

      MessageNetwork nmsg(_rx_buf[last_rx], plen, 0);
      _bus_network.send(nmsg);
      
      *cur = { (uintptr_t)_rx_buf[last_rx], 0 };
      last_rx = (last_rx+1) % desc_ring_len;
      _hwreg[RDT0] = last_rx;
    }
    
  }

  void handle_tx()
  {
    dma_desc *cur;
    while (((cur = &_tx_ring[last_tx])->hi >> 32) & 1 /* done? */) {
      uint16_t plen = cur->hi >> 32;
      msg(INFO, "TX %02x! %016llx %016llx (len %04x)\n", last_tx, cur->lo, cur->hi, plen);

      *cur = { 0, 0 };
      last_tx = (last_tx+1) % desc_ring_len;
    }
  }

  void handle_mbx()
  {
    // MBX IRQ: Just handle RESET for now. Seems like we don't
    // need more right now.
    uint32_t vmb = _hwreg[VMB];

    if (vmb & (1<<4 /* PFSTS */)) {
      // Claim message buffer
      _hwreg[VMB] = 1<<2 /* VFU */;
      if ((_hwreg[VMB] & (1<<2)) == 0) {
	msg(INFO, "MBX Could not claim buffer.\n");
	return;
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

  bool receive(MessageIrq &irq_msg)
  {
    for (unsigned i = 0; i < 3; i++)
      if (irq_msg.line == _hostirqs[i].vec) {
	unsigned eicr = _hwreg[VTEICR];
	_hwreg[VTEICR] = eicr;
	msg(IRQ, "IRQ%d RDT %04x RDH %04x TDT %04x TDH %04x\n", irq_msg.line,
	    _hwreg[RDT0], _hwreg[RDH0], _hwreg[TDT0], _hwreg[TDH0]);
	(this->*(_hostirqs[i].handle))();
	_hwreg[VTEIMS] = 7;
	return true;
      }
    return false;
  }

  bool receive(MessageNetwork &nmsg)
  {
    // Protect against our own packets. WTF?
    if ((nmsg.buffer >= (void *)_rx_buf) && (nmsg.buffer < (void *)_rx_buf[desc_ring_len])) return false;
    msg(INFO, "Send packet (size %u)\n", nmsg.len);

    // XXX Lock?
    unsigned tail = _hwreg[TDT0];
    memcpy(_tx_buf[tail], nmsg.buffer, nmsg.len);

    // If the dma descriptor is not zero, it is still in use.
    if ((_tx_ring[tail].lo | _tx_ring[tail].hi) != 0) return false;

    _tx_ring[tail] = { (uintptr_t)_tx_buf[tail], 
		       (uint64_t)nmsg.len | ((uint64_t)nmsg.len)<<46
		       | (3U<<20 /* adv descriptor */)
		       | (1U<<24 /* EOP */) | (1U<<29 /* ADESC */)
		       | (1U<<27 /* Report Status = IRQ */)  };
    msg(INFO, "TX[%02x] %016llx TDT %04x TDH %04x\n", tail, _tx_ring[tail].hi, _hwreg[TDT0], _hwreg[TDH0]);

    asm volatile ("sfence" ::: "memory");
    _hwreg[TDT0] = (tail+1) % desc_ring_len;

    return true;
  }

  Host82576VF(HostPci pci, DBus<MessageHostOp> &bus_hostop, DBus<MessageNetwork> &bus_network,
	      Clock *clock, unsigned bdf, unsigned irqs[3],
	      void *reg,
	      void *msix_reg)
    : Base82576(clock, ALL, bdf), _bus_hostop(bus_hostop), _bus_network(bus_network),
      _hwreg((volatile uint32_t *)reg), _msix_table((struct msix_table *)msix_reg), _up(false)
  {
    msg(INFO, "Found Intel 82576VF-style controller.\n");

    // Disable IRQs
    _hwreg[VTEIMC] = ~0U;
    _hwreg[VTCTRL] |= CTRL_RST;
    _hwreg[VTEIMC] = ~0U;

    // Enable MSI-X
    for (unsigned i = 0; i < 3; i++) {
      msg(INFO, "irq[%d] = 0x%x\n", i, irqs[i]);
      _msix_table[i].msg_addr = 0xFEE00000;
      _msix_table[i].msg_data = irqs[i] + 0x20;
      _msix_table[i].vector_control &= ~1;
    }
    pci.conf_write(bdf, pci.find_cap(bdf, pci.CAP_MSIX), MSIX_ENABLE);
    pci.conf_write(bdf, 0x4 /* CMD */, 1U<<2 /* Bus-Master enable */);

    // Setup IRQ mapping:
    // RX queue 0 get MSI-X vector 0, TX queue 0 gets 1.
    // Mailbox IRQs go to 2.
    _hwreg[VTIVAR]      = 0x00008180;
    _hwreg[VTIVAR_MISC] = 0x82;

    _hostirqs[0] = { irqs[0], &Host82576VF::handle_rx };
    _hostirqs[1] = { irqs[1], &Host82576VF::handle_tx };
    _hostirqs[2] = { irqs[2], &Host82576VF::handle_mbx };

    // Enable IRQs
    _hwreg[VTEIAC] = 7;		// Autoclear for all IRQs
    _hwreg[VTEIAM] = 7;
    _hwreg[VTEIMS] = 7;

    // Send RESET message
    _hwreg[VMB] = VFU;
    _hwreg[VBMEM] = VF_RESET;
    _hwreg[VMB] = Sts;

    // Set single buffer mode
    _hwreg[SRRCTL0] = 2 /* 2KB packet buffer */ | SRRCTL_DESCTYPE_ADV1B | SRRCTL_DROP_EN;

    // Enable RX
    _hwreg[RDBAL0] = (uintptr_t)_rx_ring;
    _hwreg[RDBAH0] = 0;
    _hwreg[RDLEN0] = sizeof(_rx_ring);
    msg(INFO, "%08x bytes allocated for RX descriptor ring (%d descriptors).\n", _hwreg[RDLEN0], desc_ring_len);
    _hwreg[RXDCTL0] = (1U<<25);
    assert(_hwreg[RDT0] == 0);
    assert(_hwreg[RDH0] == 0);

    // Enable TX
    _hwreg[TDBAL0] = (uintptr_t)_tx_ring;
    _hwreg[TDBAH0] = 0;
    _hwreg[TDLEN0] = sizeof(_tx_ring);
    msg(INFO, "%08x bytes allocated for TX descriptor ring (%d descriptors).\n", _hwreg[TDLEN0], desc_ring_len);
    _hwreg[TXDCTL0] = (1U<<25);
    assert(_hwreg[TDT0] == 0);
    assert(_hwreg[TDH0] == 0);

    // Prepare rings
    last_rx = last_tx = 0;
    for (unsigned i = 0; i < desc_ring_len; i++)
      _rx_ring[i] = { (uintptr_t)_rx_buf[i], 0 };

    _hwreg[TDT0] = 0;		// TDH == TDT -> queue empty
    // Tell NIC about receive descriptors.
    _hwreg[RDT0] = desc_ring_len-1;
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

    Host82576VF *dev = new Host82576VF(pci, mb.bus_hostop, mb.bus_network,
				       mb.clock(), vf_bdf,
				       irqs, reg_msg.ptr, msix_msg.ptr);

    mb.bus_hostirq.add(dev, &Host82576VF::receive_static<MessageIrq>);
    mb.bus_network.add(dev, &Host82576VF::receive_static<MessageNetwork>);
  },
  "host82576vf:parent,vf - provide driver for Intel 82576 virtual function.",
  "Example: 'host82576vf:0x100,0'");

// EOF
