// -*- Mode: C++ -*-
// Host Intel 82576 Virtual Function driver.

#include <nul/motherboard.h>
#include <host/hostvf.h>
#include <host/host82576.h>
#include <host/jsdriver.h>

static const unsigned desc_ring_len = 512;

typedef uint8 packet_buffer[2048];

struct dma_desc {
  uint64 lo;
  uint64 hi;
};

class Host82576VF : public PciDriver,
                    public Base82576,
                    public StaticReceiver<Host82576VF>
{
private:

  DBus<MessageHostOp> &_bus_hostop;
  DBus<MessageNetwork> &_bus_network;

  struct {
    unsigned vec;
    void (Host82576VF::*handle)();
  } _hostirqs[3];

  volatile uint32 *_hwreg;     // Device MMIO registers (16K)

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
      uint16 plen = cur->hi >> 32;
      //msg(INFO, "RX %02x! %016llx %016llx (len %04x)\n", last_rx, cur->lo, cur->hi, plen);
      if (handle-- == 0) {
        //msg(INFO, "Too many packets. Exit handle_rx for now.\n");
        _hwreg[VTEICS] = 1;	// XXX Needed?
        return;
      }

      MessageNetwork nmsg(_rx_buf[last_rx], plen, 0);
      _bus_network.send(nmsg);

      cur->lo = reinterpret_cast<uint64>(_rx_buf[last_rx]);
      cur->hi = reinterpret_cast<uint64>(_rx_buf[last_rx])>>32;

      last_rx = (last_rx+1) % desc_ring_len;
      _hwreg[RDT0] = last_rx;
    }

  }

  void handle_tx()
  {
    dma_desc *cur;
    while (((cur = &_tx_ring[last_tx])->hi >> 32) & 1 /* done? */) {
      //uint16 plen = cur->hi >> 32;
      //msg(INFO, "TX %02x! %016llx %016llx (len %04x)\n", last_tx, cur->lo, cur->hi, plen);

      cur->hi = cur->lo = 0;
      last_tx = (last_tx+1) % desc_ring_len;
    }
  }

  void handle_mbx()
  {
    // MBX IRQ: Just handle RESET for now. Seems like we don't
    // need more right now.
    uint32 vmb = _hwreg[VMB];

    if (vmb & (1<<4 /* PFSTS */)) {
      // Claim message buffer
      _hwreg[VMB] = 1<<2 /* VFU */;
      if ((_hwreg[VMB] & (1<<2)) == 0) {
        msg(INFO, "MBX Could not claim buffer.\n");
        return;
      }
      uint32 msg0 = _hwreg[VBMEM];
      switch (msg0 & (0xFF|CMD_ACK|CMD_NACK)) {
      case (VF_RESET | CMD_ACK):
        _mac.raw = _hwreg[VBMEM + 1];
        _mac.raw |= (static_cast<uint64>(_hwreg[VBMEM + 2]) & 0xFFFFULL) << 32;
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
        // msg(IRQ, "IRQ%d RDT %04x RDH %04x TDT %04x TDH %04x\n", irq_msg.line,
        //     _hwreg[RDT0], _hwreg[RDH0], _hwreg[TDT0], _hwreg[TDH0]);
        (this->*(_hostirqs[i].handle))();
        _hwreg[VTEIMS] = 7;
        return true;
      }
    return false;
  }

  bool receive(MessageNetwork &nmsg)
  {
    // Protect against our own packets. WTF?
    if ((nmsg.buffer >= static_cast<void *>(_rx_buf)) &&
        (nmsg.buffer < static_cast<void *>(_rx_buf[desc_ring_len]))) return false;
    //msg(INFO, "Send packet (size %u)\n", nmsg.len);

    // XXX Lock?
    unsigned tail = _hwreg[TDT0];
    memcpy(_tx_buf[tail], nmsg.buffer, nmsg.len);

    // If the dma descriptor is not zero, it is still in use.
    if ((_tx_ring[tail].lo | _tx_ring[tail].hi) != 0) return false;

    _tx_ring[tail].lo = reinterpret_cast<uint32>(_tx_buf[tail]);
    _tx_ring[tail].hi = static_cast<uint64>(nmsg.len) | (static_cast<uint64>(nmsg.len))<<46
      | (3U<<20 /* adv descriptor */)
      | (1U<<24 /* EOP */) | (1U<<29 /* ADESC */)
      | (1U<<25 /* Append MAC FCS */)
      | (1U<<27 /* Report Status = IRQ */);
    //msg(INFO, "TX[%02x] %016llx TDT %04x TDH %04x\n", tail, _tx_ring[tail].hi, _hwreg[TDT0], _hwreg[TDH0]);

    asm volatile ("sfence" ::: "memory");
    _hwreg[TDT0] = (tail+1) % desc_ring_len;

    return true;
  }

  Host82576VF(HostVfPci pci, DBus<MessageHostOp> &bus_hostop, DBus<MessageNetwork> &bus_network,
              Clock *clock, unsigned bdf, unsigned irqs[3], void *reg, uint32 itr_us)
    : PciDriver(clock, ALL, bdf), _bus_hostop(bus_hostop), _bus_network(bus_network),
      _hwreg(reinterpret_cast<volatile uint32 *>(reg)),
      _up(false)
  {
    msg(INFO, "Found Intel 82576VF-style controller.\n");

    // Disable IRQs
    _hwreg[VTEIMC] = ~0U;
    _hwreg[VTCTRL] |= CTRL_RST;
    _hwreg[VTEIMC] = ~0U;

    pci.conf_write(bdf, 1 /* CMD */, 1U<<2 /* Bus-Master enable */);

    // Setup IRQ mapping:
    // RX queue 0 get MSI-X vector 0, TX queue 0 gets 1.
    // Mailbox IRQs go to 2.
    _hwreg[VTIVAR]      = 0x00008180;
    _hwreg[VTIVAR_MISC] = 0x82;

    _hostirqs[0].vec = irqs[0]; _hostirqs[0].handle = &Host82576VF::handle_rx;
    _hostirqs[1].vec = irqs[1]; _hostirqs[1].handle = &Host82576VF::handle_tx;
    _hostirqs[2].vec = irqs[2]; _hostirqs[2].handle = &Host82576VF::handle_mbx;

    // Set IRQ throttling
    uint32 itr = (itr_us & 0xFFF) << 2;
    _hwreg[VTEITR0] = itr;
    _hwreg[VTEITR1] = itr;
    _hwreg[VTEITR2] = 0;

    if (itr == 0)
      msg(INFO, "Interrupt throttling DISABLED.\n");
    else
      msg(INFO, "Minimum IRQ interval is %dus.\n", itr_us);

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
    _hwreg[RDBAL0] = reinterpret_cast<uint64>(_rx_ring);
    _hwreg[RDBAH0] = reinterpret_cast<uint64>(_rx_ring) >> 32;
    _hwreg[RDLEN0] = sizeof(_rx_ring);
    msg(INFO, "%08x bytes allocated for RX descriptor ring (%d descriptors).\n", _hwreg[RDLEN0], desc_ring_len);
    _hwreg[RXDCTL0] = (1U<<25);
    assert(_hwreg[RDT0] == 0);
    assert(_hwreg[RDH0] == 0);

    // Enable TX
    _hwreg[TDBAL0] = reinterpret_cast<uint64>(_tx_ring);
    _hwreg[TDBAH0] = reinterpret_cast<uint64>(_tx_ring) >> 32;
    _hwreg[TDLEN0] = sizeof(_tx_ring);
    msg(INFO, "%08x bytes allocated for TX descriptor ring (%d descriptors).\n", _hwreg[TDLEN0], desc_ring_len);
    _hwreg[TXDCTL0] = (1U<<25);
    assert(_hwreg[TDT0] == 0);
    assert(_hwreg[TDH0] == 0);

    // Prepare rings
    last_rx = last_tx = 0;
    for (unsigned i = 0; i < desc_ring_len; i++) {
      _rx_ring[i].lo = reinterpret_cast<uint64>(_rx_buf[i]);
      _rx_ring[i].hi = 0;
    }

    _hwreg[TDT0] = 0;		// TDH == TDT -> queue empty
    // Tell NIC about receive descriptors.
    _hwreg[RDT0] = desc_ring_len-1;
  }

};

PARAM(host82576vf, {
    HostVfPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
    uint16 parent_bdf = argv[0];
    unsigned vf_no    = argv[1];
    uint32 itr_us     = (argv[2] == ~0U) ? 0 : argv[2] ;
    uint16 vf_bdf     = pci.vf_bdf(parent_bdf, vf_no);


    if (pci.vf_device_id(parent_bdf) != 0x10ca8086) {
      Logging::printf("Invalid device.\n");
      return;
    }

    unsigned sriov_cap = pci.find_extended_cap(parent_bdf, pci.EXTCAP_SRIOV);
    uint16 numvfs = pci.conf_read(parent_bdf, sriov_cap + 4) & 0xFFFF;
    if (vf_no >= numvfs) {
      Logging::printf("VF%d does not exist.\n", vf_no);
      return;
    }

    uint64 size;
    uint64 base = pci.vf_bar_base_size(parent_bdf, vf_no, 0, size);

    MessageHostOp reg_msg(MessageHostOp::OP_ALLOC_IOMEM, base, size);

    base = pci.vf_bar_base_size(parent_bdf, vf_no, 3, size);
    MessageHostOp msix_msg(MessageHostOp::OP_ALLOC_IOMEM, base, size);

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
    for (unsigned i = 0; i < 3; i++)
      irqs[i] = pci.get_gsi_msi(mb.bus_hostop, vf_bdf, i, msix_msg.ptr);

    Host82576VF *dev = new Host82576VF(pci, mb.bus_hostop, mb.bus_network,
                                       mb.clock(), vf_bdf,
                                       irqs, reg_msg.ptr, itr_us);

    mb.bus_hostirq.add(dev, &Host82576VF::receive_static<MessageIrq>);
    mb.bus_network.add(dev, &Host82576VF::receive_static<MessageNetwork>);
  },
  "host82576vf:parent,vf[,throttle_us] - provide driver for Intel 82576 virtual function.",
  "Example: 'host82576vf:0x100,0'");

// EOF
