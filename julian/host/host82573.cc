/** @file
 * Host Intel 82573L driver (probably works for other devices, too).
 *
 * Copyright (C) 2010-2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

/* NOTES
 *
 * This driver works for 82573L found in the ThinkPad T60/X60(s)
 * generation and 82577LM found in ThinkPad X201(s).
 *
 * Adding support for other Intel NICs should be straightforward as
 * long as their PHYs don't require initialization. We rely on
 * autoconfiguration to keep this simple. This currently does not seem
 * to work well for 82577LM.
 */

#include <nul/types.h>
#include <nul/motherboard.h>
#include <host/hostpci.h>
#include <host/jsdriver.h>
#include <service/net.h>

#include "host82573_regs.h"


enum NICType {
  INTEL_82540EM,
  INTEL_82567,
  INTEL_82573L,
  INTEL_82574,
  INTEL_82577,
  INTEL_82578,
  INTEL_82579,
};

enum Features {
  ADVANCED_QUEUE = 1U<<1,
  NO_LINK_UP     = 1U<<2,
  MASTER_DISABLE = 1U<<3,
  PHY_RESET      = 1U<<4,
  HAS_EERD       = 1U<<5,
  IVAR_4BIT      = 1U<<6,
};

struct NICInfo {
  const char *name;
  NICType type;
  uint16 devid;
  unsigned features;
};

struct DmaDesc {
  uint64 lo;
  uint64 hi;
};

typedef uint8 PacketBuffer[2048];

static const NICInfo intel_nics[] = {
  { "82578DC (b0rken?)", INTEL_82578,   0x10F0, ADVANCED_QUEUE | NO_LINK_UP | MASTER_DISABLE | PHY_RESET },
  { "82577LM",           INTEL_82577,   0x10EA, ADVANCED_QUEUE | NO_LINK_UP | MASTER_DISABLE | PHY_RESET },

  // XXX Largely untested. Check spec!
  { "82579LM",           INTEL_82579,   0x1502, ADVANCED_QUEUE | NO_LINK_UP | MASTER_DISABLE | PHY_RESET },

  { "82573L",            INTEL_82573L,  0x109A, ADVANCED_QUEUE },

  // XXX Largely untested. Check spec!
  { "82574",             INTEL_82574,  0x10D3, ADVANCED_QUEUE | MASTER_DISABLE | PHY_RESET | IVAR_4BIT},

  { "82540EM",           INTEL_82540EM, 0x100E, HAS_EERD },

  // XXX Completely untested
  { "82545EM",           INTEL_82540EM, 0x100F, HAS_EERD },

  // Seems to work fine.
  { "82567LM-2",         INTEL_82567,   0x10CC, ADVANCED_QUEUE | NO_LINK_UP | MASTER_DISABLE | PHY_RESET },

  { "82567LM-3",         INTEL_82567,   0x10DE, ADVANCED_QUEUE | NO_LINK_UP | MASTER_DISABLE | PHY_RESET },
};

class Host82573 : public PciDriver,
                  public StaticReceiver<Host82573>
{
  static const unsigned desc_ring_len = 512;

  const NICInfo &_info;
  EthernetAddr   _mac;

  DBus<MessageHostOp>      &_bus_hostop;
  DBus<MessageNetwork>     &_bus_network;

  volatile uint32 *_hwreg;

  bool             _multi_irq_mode; // True for MSI-X
  unsigned         _hostirq;        // In multi_irq_mode used only for stuff not RX/TX related.

  bool             _rxo_warned;	// Have we warned about receiver overrun yet?

  enum {
    IRQ_MISC = 0,
    IRQ_RX   = 1,
    IRQ_TX   = 2,
  };

  unsigned         _hostirq_rx;
  unsigned         _hostirq_tx;

  uint16        _rx_last;
  DmaDesc      *_rx_ring;

  uint16        _tx_last;
  uint16        _tx_tail;	// Mirrors TDT
  DmaDesc      *_tx_ring;

  PacketBuffer _rx_buf[desc_ring_len];
  PacketBuffer _tx_buf[desc_ring_len];

  // Test for a specific feature
  bool feature(unsigned bit) { return (_info.features & bit) != 0; }

  // Reset MAC and (if supported and straightforward) PHY as well.
  void mac_reset()
  {
    if (feature(MASTER_DISABLE)) {
      _hwreg[CTRL] |= CTRL_MASTER_DISABLE;
      if (!wait(_hwreg[STATUS], STATUS_MASTER_ENABLE_STATUS, 0))
        Logging::panic("Device hang?");
    }

    uint32 rst = CTRL_SWRST;
    if (feature(PHY_RESET)) rst |= CTRL_PHY_RST;

    _hwreg[CTRL] |= rst;
    spin(1000);		// Wait 1ms
    if (!wait(_hwreg[CTRL], rst, 0))
      Logging::panic("Reset failed!");
    _hwreg[IMC] = ~0UL;
    msg(INFO, "SWRST complete.\n");
  }

  // Force link up. Not needed on some models.
  void mac_set_link_up()
  {
    if (not feature(NO_LINK_UP))
      _hwreg[CTRL] |= CTRL_SLU;
  }

  void tx_configure()
  {
    _tx_ring = new(256) DmaDesc[desc_ring_len];

    _hwreg[TCTL] &= ~(1<<1 /* Enable */);
    _hwreg[TIDV] = 8;        // Need to set this to something non-zero.
    mword phys_tx_ring = addr2phys(_tx_ring);
    _hwreg[TDBAL] = static_cast<uint64>(phys_tx_ring) & 0xFFFFFFFFU;
    _hwreg[TDBAH] = static_cast<uint64>(phys_tx_ring) >> 32;
    _hwreg[TDLEN] = sizeof(DmaDesc)*desc_ring_len;
    _hwreg[TDT] = 0;
    _hwreg[TDH] = 0;

    // Assume some sensible defaults
    _hwreg[TCTL]  |= (1<<1 /* Enable */) | (1<<3 /* Pad */);
    //_hwreg[TXDCTL] = 0;
  }

  void rx_configure()
  {
    _rx_ring = new(256) DmaDesc[desc_ring_len];

    _hwreg[RCTL]  &= ~(1<<1 /* Enable */);
    _hwreg[RXDCTL] = 0;       // The spec says so: 11.2.1.3.13. Only
                              // descriptors with RS are written back!
    _hwreg[RADV]  = 8;	// RX IRQ delay 8ms (µs?!)
    mword phys_rx_ring = addr2phys(_rx_ring);
    _hwreg[RDBAL] = static_cast<uint64>(phys_rx_ring) & 0xFFFFFFFFU;
    _hwreg[RDBAH] = static_cast<uint64>(phys_rx_ring) >> 32;
    _hwreg[RDLEN] = sizeof(DmaDesc)*desc_ring_len;
    _hwreg[RDT]   = 0;
    _hwreg[RDH]   = 0;
    _hwreg[RDTR]  = 0;       // Linux source warns that Rx will likely
                             // hang, if this is not zero.
    if (feature(ADVANCED_QUEUE)) {
      _hwreg[RXCSUM] = (1<<8 /* IP Checksum */) | (1<<9 /* TCP/UDP Checksum */);
      _hwreg[RFCTL] = (1<<15 /* Extended Status (advanced descriptors) */)
        | (1<<14 /* IP Fragment Split Disable */);
    }

    _hwreg[RCTL]  = (1<<1 /* Enable */) | (1<<15 /* Broadcast accept */) | (1<<26 /* Strip FCS */)
      // XXX Enable promiscuous mode. We could avoid this by passing
      // client MAC addresses to the hardware filters. Shouldn't be a
      // problem in a switched Ethernet, though.
      | (1<<3 /* Unicast promiscuous */);

    // Add descriptors
    for (unsigned i = 0; i < desc_ring_len - 1; i++) {
      _rx_ring[i].lo = addr2phys(_rx_buf[i]);
      _rx_ring[i].hi = 0;

      // Tell NIC about receive descriptor.
      _hwreg[RDT] = i+1;
    }
  }

  bool rx_desc_done(DmaDesc &desc)
  {
    uint64 mask = feature(ADVANCED_QUEUE) ? 0x1 : 0x1ULL<<32;
    return (desc.hi & mask) != 0;
  }

  uint16 rx_desc_size(DmaDesc &desc)
  {
    unsigned shift = feature(ADVANCED_QUEUE) ? 32 : 0;
    return (desc.hi >> shift) & 0xFFFF;
  }

  bool tx_desc_done(DmaDesc &desc)
  {
    return (desc.hi & (0x1ULL<<32)) != 0;
  }

  // Consume and distribute all received packets from the RX queue.
  void rx_handle()
  {
    unsigned quota = desc_ring_len/2;

    // Process all filled RX buffers
    while (--quota and rx_desc_done(_rx_ring[_rx_last])) {
      // Propagate packet
      //rx_packet_process(_rx_buf[_rx_last], rx_desc_size(_rx_ring[_rx_last]));

      uint16 plen = _rx_ring[_rx_last].hi >> 32;
      // Logging::printf("RX %016llx %016llx\n", _rx_ring[_rx_last].lo, _rx_ring[_rx_last].hi);
      // Logging::printf("   plen %u\n", plen);
      assert(plen <= 2048);

      MessageNetwork nmsg(_rx_buf[_rx_last], plen, 0);
      _bus_network.send(nmsg);

      _rx_ring[_rx_last].lo = 0;
      _rx_ring[_rx_last].hi = 0;

      _rx_last = (_rx_last+1) % desc_ring_len;

      // XXX Use shadow RDT
      unsigned rdt = _hwreg[RDT];
      _rx_ring[rdt].lo = addr2phys(_rx_buf[rdt]);
      _rx_ring[rdt].hi = 0;
      _hwreg[RDT] = (rdt+1) % desc_ring_len;
    }

    if (quota == 0) {
      // Processed too many descriptors. Give other code a chance to
      // do something useful. Reraise IRQ to be scheduled again later.
      _hwreg[ICS] = _multi_irq_mode ? (1U << 20 /* RX0 */) : ICR_RXT;
    }
  }

  // Returns the index into the (real) TX queue
  // unsigned tx_send(SimpleTxDesc &desc, mword offset)
  // {
  //   DmaDesc      &hw_desc = _tx_ring[_tx_tail];

  //   hw_desc.lo = addr2phys(desc.buf + offset);

  //   // Legacy descriptor
  //   hw_desc.hi = desc.len
  //     | (1U << (24+0) /* EOP */)
  //     | (1U << (24+1) /* FICS */)
  //     | (1U << (24+3) /* RS */);

  //   unsigned old_tail = _tx_tail;
  //   _tx_tail   = (_tx_tail+1)%desc_ring_len;
  //   return old_tail;
  // }

  uint16 mac_eeprom_read(uint16 addr)
  {
    assert(feature(HAS_EERD));

    uint32 start, mask;

    if (_info.type == INTEL_82540EM) {
      // XXX This is reverse engineered from qemu. Is there a spec somewhere?
      start = addr<<8 | 1; mask = 0x10;
    } else
      Logging::panic("No idea how to drive EEPROM on this NIC.\n");

    _hwreg[EERD] = start;
    if (!wait(_hwreg[EERD], mask, mask))
      Logging::panic("EEPROM read timed out.\n");

    return _hwreg[EERD] >> 16;
  }

  // Figure out the device's native MAC address.
  EthernetAddr mac_get_ethernet_addr()
  {
    // We assume that if RA0 has its valid bit set we can use it
    // as-is. Otherwise check the EEPROM.

    unsigned rah0 = _hwreg[RAH0];
    if ((rah0 & (1U<<31)) != 0) {
      msg(INFO, "MAC found in RA0.\n");
      return EthernetAddr(_hwreg[RAL0] | ((static_cast<uint64>(rah0) & 0xFFFF) << 32));
    } else if (feature(HAS_EERD)) {
      msg(INFO, "Querying EEPROM for MAC.\n");
      uint16 word[] = { mac_eeprom_read(0),
                        mac_eeprom_read(1),
                        mac_eeprom_read(2) };

      return EthernetAddr(word[0] & 0xFF, word[0] >> 8,
                          word[1] & 0xFF, word[1] >> 8,
                          word[2] & 0xFF, word[2] >> 8);
    } else {
      msg(WARN,"Warning: Don't know how to figure out MAC address.\n");
      return EthernetAddr(0);
    }
  }

  void tx_handle()
  {
    DmaDesc *cur;
    while (((cur = &_tx_ring[_tx_last])->hi >> 32) & 1 /* done? */) {
      //uint16 plen = cur->hi >> 32;
      //msg(INFO, "TX %02x! %016llx %016llx (len %04x)\n", _tx_last, cur->lo, cur->hi, plen);

      cur->hi = cur->lo = 0;
      _tx_last = (_tx_last+1) % desc_ring_len;
    }
  }

  void log_irq_status(unsigned irq) { log_irq_status(irq, _hwreg[ICR]); }
  void log_irq_status(unsigned irq, uint32 icr)
  {
    // msg(IRQ, "IRQ %02x%s: %08x %08x %08x | RDT %04x | RDH %04x | TDT %04x | TDH %04x\n",
    //     irq, _multi_irq_mode ? " X" : "",
    //     _hwreg[STATUS], icr, _hwreg[IMS], _hwreg[RDT], _hwreg[RDH], _hwreg[TDT], _hwreg[TDH]);
  }

public:

  void misc_handle(uint32 icr) {
    if (icr & ICR_LSC)
      msg(INFO, "Link is %s.\n", ((_hwreg[STATUS] & STATUS_LU) != 0) ? "UP" : "DOWN");

    if ((icr & ICR_RXO) and not not _rxo_warned) {
      msg(WARN, "Receiver overrun. Maybe RX queue is too short. Report this.\n");
      _rxo_warned = true;
    }
  }

  bool receive(MessageIrq &irq_msg)
  {
    if (_multi_irq_mode) {
      if (irq_msg.line != _hostirq    and
          irq_msg.line != _hostirq_rx and
          irq_msg.line != _hostirq_tx) return false;

      // Don't believe this too much. Reading ICR might be racy.
      //log_irq_status(irq_msg.line);

      // MSI-X mode, autoclear
      if (irq_msg.line == _hostirq) {
        _hwreg[IMS] = (1U << 24 /* Other */);
        uint32 icr = _hwreg[ICR];
        log_irq_status(irq_msg.line, icr);
        misc_handle(icr);
      } else if (irq_msg.line == _hostirq_tx) {
        _hwreg[IMS] = (1U << 22 /* TX0 */);
        tx_handle();
      } else if (irq_msg.line == _hostirq_rx) {
        _hwreg[IMS] = (1U << 20 /* RX0 */);
        rx_handle();
      } else Logging::panic("?");
    } else {
      // Legacy/MSI mode
      if (irq_msg.line != _hostirq || irq_msg.type != MessageIrq::ASSERT_IRQ)  return false;

      uint32 icr = _hwreg[ICR];

      log_irq_status(irq_msg.line, icr);

      // If the interrupt is not asserted, ICR has not autocleared and
      // we need to clear it manually.
      if ((icr & ICR_INTA) == 0) _hwreg[ICR] = icr;

      misc_handle(icr);

      if (icr & ICR_TXDW) {
        // TX descriptor writeback
        tx_handle();
      }

      if (icr & ICR_RXT) {
        // RX timer expired
        rx_handle();
      }
    }

    return true;
  }

  bool receive(MessageNetwork &nmsg)
  {
    switch (nmsg.type) {
    case MessageNetwork::QUERY_MAC:
      nmsg.mac = Endian::hton64(_mac.raw) >> 16;
      return true;
    case MessageNetwork::PACKET:
        // Protect against our own packets. WTF?
        if ((nmsg.buffer >= static_cast<void *>(_rx_buf[0])) &&
            (nmsg.buffer < static_cast<void *>(_rx_buf[desc_ring_len]))) return false;
        //msg(INFO, "Send packet (size %u)\n", nmsg.len);

        {
          unsigned tail = _hwreg[TDT];
          memcpy(_tx_buf[tail], nmsg.buffer, nmsg.len);

          // If the dma descriptor is not zero, it is still in use.
          if ((_tx_ring[tail].lo | _tx_ring[tail].hi) != 0)  {
            msg(INFO, "Descriptor still in use. Drop packet.\n");
            return false;
          }

          _tx_ring[tail].lo = addr2phys(_tx_buf[tail]);
          _tx_ring[tail].hi = static_cast<uint64>(nmsg.len)
            | (1U<<24 /* EOP */)
            | (1U<<25 /* Append MAC FCS */)
            | (1U<<27 /* Report Status = IRQ */);

          //msg(INFO, "TX[%02x] %016llx TDT %04x TDH %04x\n", tail, _tx_ring[tail].hi, _hwreg[TDT], _hwreg[TDH]);

          MEMORY_BARRIER;
          _hwreg[TDT] = (tail+1) % desc_ring_len;
        }

      return true;
    default:
      return false;
    }
  }

  void enable_irqs()
  {
    if (_multi_irq_mode) {
      if (feature(IVAR_4BIT)) {
        _hwreg[IMS] =
          ICR_LSC |
          (1U << 20 /* RX0 */) | (1U << 22 /* TX0 */) | (1U << 24 /* Other */);
        _hwreg[ICS] = ICR_LSC | (1U << 24); // Force LSC IRQ. (For logging purposes only.)
      } else Logging::panic("?");
    } else {
      // Legacy/MSI
      _hwreg[IMS] = ICR_LSC | ICR_TXDW | ICR_RXT | ICR_RXO;
      _hwreg[ICS] = ICR_LSC; // Force LSC IRQ. (For logging purposes only.)
    }
  }

  Host82573(unsigned vnet, HostPci pci, DBus<MessageHostOp> &bus_hostop,
            DBus<MessageNetwork> &bus_network, DBus<MessageAcpi> &bus_acpi,
            Clock *clock, unsigned bdf, const NICInfo &info)
    : PciDriver("82573", bus_hostop, clock, ALL, bdf),
      _info(info),
      _bus_hostop(bus_hostop), _bus_network(bus_network), _rxo_warned(false),
      _rx_last(0), _tx_last(0), _tx_tail(0)
  {
    msg(INFO, "Type: %s\n", info.name);
    if (info.type == INTEL_82540EM) msg(WARN, "This NIC has only been tested in QEMU.\n");
    if (info.type == INTEL_82574)   msg(WARN, "This NIC has only been tested in VMWare.\n");

    // Try to map the device into our address space (IO-MMU)
    assign_pci();

    for (unsigned i = HostPci::BAR0; i < HostPci::BAR0 + HostPci::MAX_BAR; i++) {
      bool is64bit = false;
      unsigned type;
      uint64 base = pci.bar_base(bdf, i, &type);
      uint64 size = pci.bar_size(bdf, i, &is64bit);

      switch (type) {
      case HostPci::BAR_IO:
        break;
      case HostPci::BAR_TYPE_32B:
      case HostPci::BAR_TYPE_64B: {
        MessageHostOp reg_msg(MessageHostOp::OP_ALLOC_IOMEM, base, size);
        if (!_hwreg) {
          if (!bus_hostop.send(reg_msg))
            Logging::panic("Could not map register window.");
          _hwreg = reinterpret_cast<volatile uint32 *>(reg_msg.ptr);
          goto hwreg_done;
        }
      }
        break;
      default:
        msg(WARN, "WARNING: Unknown BAR.\n");
      }

      i += is64bit ? 1 : 0;
    }
  hwreg_done:

    msg(INFO, "Registers at %p\n", _hwreg);
    // XXX Proper bailout?
    assert(_hwreg);

    _hwreg[IMC] = ~0UL;

    mac_reset();
    msg(INFO, "Set Bus Master Enable, Mem/IO Enable.\n");
    pci.conf_write(bdf, 1 /* CMD */,
                   pci.conf_read(bdf, 1) | 0x7);

    _mac = mac_get_ethernet_addr();
    msg(INFO, "We are " MAC_FMT "\n", MAC_SPLIT(&_mac));

    // We assume that the other RA registers have been disabled by the
    // software reset. (The spec says so.)
    _hwreg[RAL0] = _mac.raw & 0xFFFFFFFFU;
    _hwreg[RAH0] = 1ULL<<31 | _mac.raw >> 32;

    tx_configure();
    rx_configure();

    // Multicast Table
    for (unsigned i = 0; i < (0x80/4); i++)
      _hwreg[MTA + i] = 0U;

    // Configure IRQ logic.
    if (pci.find_cap(bdf, HostPci::CAP_MSIX)) {
      _multi_irq_mode = true;

      static_assert((IRQ_MISC | IRQ_RX | IRQ_TX) < 4, "Misconfiguration");
      _hwreg[CTRL_EXT] |= (1U << 31 /* PBA enable */) | (1U << 24 /* EIAME */) | (1U << 27 /* IAME */) | (1U << 22 /* ?? */);
      _hwreg[CTRL]     |= (1U << 30 /* VME */);

      _hostirq    = pci.get_gsi_msi(bus_hostop, bdf, IRQ_MISC);
      _hostirq_rx = pci.get_gsi_msi(bus_hostop, bdf, IRQ_RX);
      _hostirq_tx = pci.get_gsi_msi(bus_hostop, bdf, IRQ_TX);
      msg(INFO, "MSI-X mode: MISC=%x RX=%x TX=%x\n",
          _hostirq, _hostirq_rx, _hostirq_tx);

      // Program IRQs and set ICR to autoclear for all
      if (feature(IVAR_4BIT)) {
        _hwreg[IVAR] =
          1U<<31 |              // Always send IRQ on writeback
          ((8 | IRQ_RX) << 0) |
          ((8 | IRQ_TX) << 8) |
          ((8 | IRQ_MISC) << 16);


        _hwreg[EIAC] =
          (1U << 20 /* RX0 */) | (1U << 22 /* TX0 */) | (1U << 24 /* Other */);

        _hwreg[IAM] = 0xFFFFFFFFU;
        //_hwreg[ICS] = (1U << 24);
      } else {
        Logging::panic("We want to use MSI-X, but cannot figure out how to program IRQ allocation.");
      }


    } else {
      // MSI or legacy interrupts

      _hostirq = pci.get_gsi(bus_hostop, bus_acpi, bdf, 0);
      msg(INFO, "Our IRQ is 0x%x. Legacy/MSI mode.\n", _hostirq);
    }

    _hwreg[CTRL_EXT] |= (1<<28 /* Driver loaded */);

    mac_set_link_up();
  }
};

PARAM_HANDLER(host82573,
              "host82573:instance=0,vnet - provide driver for Intel 82573L Ethernet controller.",
              "Example: 'host82573")
{
  HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
  unsigned found = 0;
  unsigned instance = (~argv[0] == 0) ? 0 : argv[0];

  for (unsigned bdf, num = 0; (bdf = pci.search_device(0x2, 0x0, num++));) {
    unsigned cfg0 = pci.conf_read(bdf, 0x0);
    if ((cfg0 & 0xFFFF) != 0x8086 /* INTEL */) continue;

    // Find specific device
    unsigned i;
    for (i = 0; i < (sizeof(intel_nics)/sizeof(NICInfo)); i++) {
      if ((cfg0>>16 == intel_nics[i].devid) && (found++ == instance)) {
        Host82573 *dev = new Host82573(argv[1], pci, mb.bus_hostop, mb.bus_network,
                                       mb.bus_acpi,
                                       mb.clock(), bdf, intel_nics[i]);
        mb.bus_hostirq.add(dev, &Host82573::receive_static<MessageIrq>);
        mb.bus_network.add(dev, &Host82573::receive_static<MessageNetwork>);
        dev->enable_irqs();
      }
    }
  }
}

// EOF
