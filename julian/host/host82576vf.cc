/** @file
 * Host Intel 82576 Virtual Function driver.
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL.
 *
 * NUL is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 * License version 2 for more details.
 */

#include <nul/motherboard.h>
#include <nul/compiler.h>
#include <host/hostvf.h>
#include <host/host82576.h>
#include <host/jsdriver.h>

#include <service/net.h>

static const unsigned desc_ring_len = 32;

typedef uint8 packet_buffer[2048];

struct dma_desc {
  uint64 lo;
  uint64 hi;
};

static_assert((sizeof(dma_desc[desc_ring_len]) & 0x7F) == 0,
	      "Size of DMA descriptors must be 128-byte aligned.");

class Host82576VF : public PciDriver,
                    public Base82576VF,
		    public Base82576,
                    public StaticReceiver<Host82576VF>
{
private:
  DBus<MessageNetwork> &_bus_network;

  unsigned _hostirqs[2];

  volatile uint32 *_hwreg;     // Device MMIO registers (16K)

  EthernetAddr _mac;

  bool _up;			// Are we UP?

  unsigned last_rx;
  dma_desc *_rx_ring;

  unsigned last_tx;
  dma_desc *_tx_ring;

  packet_buffer _rx_buf[desc_ring_len];
  packet_buffer _tx_buf[desc_ring_len];

  const bool _promisc;
  
public:

  void reset_complete()
  {
    if (_up)
      msg(INFO, "Another reset?!\n");

    _up = true;

    // Enable receive
    _hwreg[RXDCTL0] |= (1<<25);

    if (_promisc) {
      msg(INFO, "Asking to be promiscuous.\n");
      _hwreg[VMB] = VFU;
      _hwreg[VBMEM] = VF_SET_PROMISC | VF_SET_PROMISC_UNICAST;
      _hwreg[VMB] = Sts;
    }
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

      cur->lo = 0;
      cur->hi = 0; 

      last_rx = (last_rx+1) % desc_ring_len;

      // XXX Use shadow RDT
      unsigned rdt = _hwreg[RDT0];
      _rx_ring[rdt].lo = reinterpret_cast<mword>(_rx_buf[rdt]);
      _rx_ring[rdt].hi = 0;
      _hwreg[RDT0] = (rdt+1) % desc_ring_len;
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

  void handle_rxtx()
  {
    handle_rx();
    handle_tx();
    _hwreg[VTEIMS] = 1;
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
    if (irq_msg.line == _hostirqs[0]) {
      handle_rxtx();
      _hwreg[VTEIMS] = 1;
    } else if (irq_msg.line == _hostirqs[1]) {
      handle_mbx();
      _hwreg[VTEIMS] = 1<<1;
    } else
      return false;

    return true;
  }

  bool receive(MessageNetwork &nmsg)
  {
    switch (nmsg.type) {
    case MessageNetwork::QUERY_MAC:
      nmsg.mac = Endian::hton64(_mac.raw) >> 16;
      return true;
    case MessageNetwork::PACKET:
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

        MEMORY_BARRIER;
        _hwreg[TDT0] = (tail+1) % desc_ring_len;
        return true;
      }
    default:
      return false;
    }
  }

  void enable_irqs()
  {
    _hwreg[VTEIMS] = 3;
  }

  Host82576VF(HostVfPci pci, DBus<MessageHostOp> &bus_hostop,
              DBus<MessageNetwork> &bus_network, Clock *clock,
	      unsigned bdf, unsigned irqs[2], void *reg, uint32 itr_us, bool promisc)
    : PciDriver("82576VF", bus_hostop, clock, ALL, bdf), _bus_network(bus_network),
      _hwreg(reinterpret_cast<volatile uint32 *>(reg)),
      _up(false), _promisc(promisc)
  {
    msg(INFO, "Found Intel 82576VF-style controller.\n");

    // Disable IRQs and reset
    _hwreg[VTEIMC] = ~0U;
    _hwreg[VTCTRL] |= CTRL_RST;
    _hwreg[VTEIMC] = ~0U;

    pci.conf_write(bdf, 1 /* CMD */, 1U<<2 /* Bus-Master enable */);

    // Setup IRQ mapping:
    // RX and TX trigger MSI 0.
    // Mailbox IRQs trigger MSI 1
    _hwreg[VTIVAR]      = 0x00008080;
    _hwreg[VTIVAR_MISC] = 0x81;

    for (unsigned i = 0; i < 2; i++)
      _hostirqs[i] = irqs[i];

    // Set IRQ throttling
    uint32 itr = (itr_us & 0xFFF) << 2;
    _hwreg[VTEITR0] = itr;
    _hwreg[VTEITR1] = itr;
    _hwreg[VTEITR2] = 0;

    if (itr == 0)
      msg(INFO, "Interrupt throttling DISABLED.\n");
    else
      msg(INFO, "Minimum IRQ interval is %dus.\n", itr_us);

    // Setup autoclear and stuff for both IRQs.
    _hwreg[VTEIAC] = 3;
    _hwreg[VTEIAM] = 3;

    // Set single buffer mode
    _hwreg[SRRCTL0] = 2 /* 2KB packet buffer */ | SRRCTL_DESCTYPE_ADV1B | SRRCTL_DROP_EN;

    // Enable RX
    _rx_ring = new(128) dma_desc[desc_ring_len];
    _hwreg[RDBAL0] = reinterpret_cast<mword>(_rx_ring);
    _hwreg[RDBAH0] = 0; //reinterpret_cast<mword>(_rx_ring) >> 32;
    _hwreg[RDLEN0] = sizeof(dma_desc[desc_ring_len]);
    msg(INFO, "%08x bytes allocated for RX descriptor ring (%d descriptors).\n", _hwreg[RDLEN0], desc_ring_len);
    assert(_hwreg[RDT0] == 0);
    assert(_hwreg[RDH0] == 0);
    _hwreg[RXDCTL0] = (1U<<25 /* Enable */);
    msg(INFO, "RDBAL %08x RDBAH %08x RXDCTL %08x\n", _hwreg[RDBAL0], _hwreg[RDBAH0], _hwreg[RXDCTL0]);


    // Enable TX
    _tx_ring = new(128) dma_desc[desc_ring_len];
    _hwreg[TDBAL0] = reinterpret_cast<mword>(_tx_ring);
    _hwreg[TDBAH0] = 0; //reinterpret_cast<mword>(_tx_ring) >> 32;
    _hwreg[TDLEN0] = sizeof(dma_desc[desc_ring_len]);
    msg(INFO, "%08x bytes allocated for TX descriptor ring (%d descriptors).\n", _hwreg[TDLEN0], desc_ring_len);
    _hwreg[TXDCTL0] = (1U<<25);
    assert(_hwreg[TDT0] == 0);
    assert(_hwreg[TDH0] == 0);

    // Prepare rings
    last_rx = last_tx = 0;

    _hwreg[TDT0] = 0;		// TDH == TDT -> queue empty
    _hwreg[RDT0] = 0;		// RDH == RDT -> queue empty

    for (unsigned i = 0; i < desc_ring_len - 1; i++) {
      _rx_ring[i].lo = reinterpret_cast<mword>(_rx_buf[i]);
      _rx_ring[i].hi = 0;

      // Tell NIC about receive descriptor.
      _hwreg[RDT0] = i+1;
    }

    // Send RESET message
    _hwreg[VMB] = VFU;
    _hwreg[VBMEM] = VF_RESET;
    _hwreg[VMB] = Sts;


    // Get each IRQ once.
    //_hwreg[VTEICS] = 3;
  }

  ~Host82576VF() {
    _hwreg[VTEIMC] = ~0U;	// Disable IRQs
    _hwreg[RXDCTL0] = 0;	// Disable queues.
    _hwreg[TXDCTL0] = 0;

    delete[] _tx_ring;
    delete[] _rx_ring;
  }

};

PARAM_HANDLER(host82576vf,
	      "host82576vf:pf,vf,[promisc=no[,throttle_us=off]] - provide driver for Intel 82576 virtual function.",
	      "Example: 'host82576vf:0,0'")
{
  HostVfPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
  unsigned vf_no    = argv[1];
  bool   promisc    = (argv[2] == ~0U) ? 0 : argv[2] ;
  uint32 itr_us     = (argv[3] == ~0U) ? 0 : argv[3] ;

  // Find parent BDF
  uint16 parent_bdf = 0;
  unsigned found = 0;

  for (unsigned bdf, num = 0; (bdf = pci.search_device(0x2, 0x0, num++));) {
    unsigned cfg0 = pci.conf_read(bdf, 0x0);
    if (cfg0 == 0x10c98086) {
      if (found++ == argv[0]) {
	parent_bdf = bdf;
	break;
      }
    }
  }

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

  uint64 size = 0;
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
  MessageHostOp assign_msg(MessageHostOp::OP_ASSIGN_PCI, vf_bdf, parent_bdf);
  if (!mb.bus_hostop.send(assign_msg)) {
    Logging::printf("Could not assign PCI device.\n");
    return;
  }

  // IRQs
  unsigned irqs[2];
  for (unsigned i = 0; i < 2; i++)
    irqs[i] = pci.get_gsi_msi(mb.bus_hostop, vf_bdf, i, msix_msg.ptr);

  Host82576VF *dev = new Host82576VF(pci, mb.bus_hostop, mb.bus_network,
				     mb.clock(), vf_bdf,
				     irqs, reg_msg.ptr, itr_us,
				     promisc);

  mb.bus_hostirq.add(dev, &Host82576VF::receive_static<MessageIrq>);
  mb.bus_network.add(dev, &Host82576VF::receive_static<MessageNetwork>);

  dev->enable_irqs();
}

// EOF
