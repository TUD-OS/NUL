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
 * generation and 82577LM found in ThinkPad X201(s). It is believed to
 * work on Qemu's default e1000 (82540EM) as well.
 *
 * Adding support for other Intel NICs should be straightforward as
 * long as their PHYs don't require initialization. We rely on
 * autoconfiguration to keep this simple.
 */

#include <nul/types.h>
#include <nul/motherboard.h>
#include <host/hostpci.h>
#include <host/jsdriver.h>
#include <service/net.h>

#include "host82573_regs.h"


enum NICType {
  INTEL_82540EM,
  INTEL_82573L,
  INTEL_82577,
  INTEL_82578,
};

enum Features {
  ADVANCED_QUEUE = 1U<<1,
  NO_LINK_UP     = 1U<<2,
  MASTER_DISABLE = 1U<<3,
  PHY_RESET      = 1U<<4,
  HAS_EERD       = 1U<<5,
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
  { "82573L",            INTEL_82573L,  0x109A, ADVANCED_QUEUE },
  { "82540EM",           INTEL_82540EM, 0x100E, HAS_EERD },
};

class Host82573 : public PciDriver,
		  public StaticReceiver<Host82573>
{
  const NICInfo &_info;
  EthernetAddr   _mac;

  const unsigned _tx_ring_len_shift;
  const unsigned _rx_ring_len_shift;

  DBus<MessageHostOp>      &_bus_hostop;

  volatile uint32 *_hwreg;
  unsigned         _hostirq;

  uint16        _rx_last;
  DmaDesc      *_rx_ring;

  uint16        _tx_last;
  uint16        _tx_tail;	// Mirrors TDT
  DmaDesc      *_tx_ring;

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
    _tx_ring = new(256) DmaDesc[1 << _tx_ring_len_shift];

    _hwreg[TCTL] &= ~(1<<1 /* Enable */);
    _hwreg[TIDV] = 8;	     // Need to set this to something non-zero.
    mword phys_tx_ring = addr2phys(_tx_ring);
    _hwreg[TDBAL] = static_cast<uint64>(phys_tx_ring) & 0xFFFFFFFFU;
    _hwreg[TDBAH] = static_cast<uint64>(phys_tx_ring) >> 32;
    _hwreg[TDLEN] = sizeof(DmaDesc) << _tx_ring_len_shift;
    _hwreg[TDT] = 0;
    _hwreg[TDH] = 0;
    _hwreg[TCTL] = (1<<1 /* Enable */) | (1<<3 /* Pad */);
  }

  void rx_configure()
  {
    _rx_ring = new(256) DmaDesc[1 << _rx_ring_len_shift];

    _hwreg[RCTL]  &= ~(1<<1 /* Enable */);
    _hwreg[RXDCTL] = 0;	      // The spec says so: 11.2.1.3.13. Only
			      // descriptors with RS are written back!
    _hwreg[RADV]  = 8;	// RX IRQ delay 8ms (µs?!)
    mword phys_rx_ring = addr2phys(_rx_ring);
    _hwreg[RDBAL] = static_cast<uint64>(phys_rx_ring) & 0xFFFFFFFFU;
    _hwreg[RDBAH] = static_cast<uint64>(phys_rx_ring) >> 32;
    _hwreg[RDLEN] = sizeof(DmaDesc) << _rx_ring_len_shift;
    _hwreg[RDT]   = 0;
    _hwreg[RDH]   = 0;
    _hwreg[RDTR]  = 0;	     // Linux source warns that Rx will likely
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
    // unsigned processed = 0;

    // Process all filled RX buffers
    // while (rx_desc_done(_rx_ring[_rx_last])) {
    //   // Propagate packet
    //   rx_packet_process(_rx_buf[_rx_last], rx_desc_size(_rx_ring[_rx_last]));
      
    //   // Rearm RX descriptor
    //   _rx_ring[_rx_last].lo = _rx_buf_phys[_rx_last];
    //   _rx_ring[_rx_last].hi = 0;

    //   processed += 1;
    //   _rx_last = (_rx_last+1)%desc_ring_len;
    // }

    // // Enqueue as many buffers as we consumed.
    // _hwreg[RDT] = (_hwreg[RDT] + processed) % desc_ring_len;

    // if (processed != 0)
    //   msg(RX, "Processed %d packet%s.\n", processed, (processed == 1) ? "" : "s");

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

public:

  bool receive(MessageIrq &irq_msg)
  {
    if (irq_msg.line != _hostirq || irq_msg.type != MessageIrq::ASSERT_IRQ)  return false;
    
    uint32 icr = _hwreg[ICR];

    msg(IRQ, "%08x %08x %08x | RDT %04x | RDH %04x | TDT %04x | TDH %04x\n",
     	_hwreg[STATUS], icr, _hwreg[IMS], _hwreg[RDT], _hwreg[RDH], _hwreg[TDT], _hwreg[TDH]);

    // If the interrupt is not asserted, ICR has not autocleared and
    // we need to clear it manually.
    if ((icr & ICR_INTA) == 0) _hwreg[ICR] = icr;

    if (icr & ICR_LSC)
      msg(INFO, "Link is %s.\n", ((_hwreg[STATUS] & STATUS_LU) != 0) ? "UP" : "DOWN");
   
    if (icr & ICR_RXO) {
      msg(WARN, "Receiver overrun. Report this.\n");
      // Mask this, so we don't spam the console.
      _hwreg[IMC] = ICR_RXO;
    }

    if (icr & ICR_TXDW) {
      // TX descriptor writeback 

    }

    if (icr & ICR_RXT) {
      // RX timer expired
      rx_handle();
    }

    return true;
  }

  Host82573(unsigned vnet, HostPci pci, DBus<MessageHostOp> &bus_hostop,
	    DBus<MessageAcpi> &bus_acpi,
	    Clock *clock, unsigned bdf, const NICInfo &info)
    : PciDriver("82573", bus_hostop, clock, ALL, bdf), 
      _info(info),
      _tx_ring_len_shift(10), _rx_ring_len_shift(10),
      _bus_hostop(bus_hostop), _rx_last(0), _tx_last(0), _tx_tail(0)
  {
    msg(INFO, "Type: %s\n", info.name);
    if (info.type == INTEL_82540EM) msg(WARN, "This NIC has only been tested in QEMU.\n");

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

    _mac = mac_get_ethernet_addr();
    msg(INFO, "We are " MAC_FMT "\n", MAC_SPLIT(&_mac));
    // We assume that the other RA registers have been disabled by the
    // software reset. (The spec says so.)
    // _hwreg[RAL0] = _mac.raw & 0xFFFFFFFFU;
    // _hwreg[RAH0] = 1ULL<<31 | _mac.raw >> 32;
    // XXX Disable all address filtering. We use promiscuous mode.
    _hwreg[RAH0] = 0;
    
    tx_configure();
    rx_configure();

    // Multicast Table
    for (unsigned i = 0; i < (0x80/4); i++)
      _hwreg[MTA + i] = 0U;

    // Configure IRQ logic.
    _hostirq = pci.get_gsi(bus_hostop, bus_acpi, bdf, 0);
    _hwreg[CTRL_EXT] |= (1<<29 /* Clear timers on IRQ */) | (1<<28 /* Driver loaded */);
    _hwreg[IMS] = ICR_LSC | ICR_TXDW | ICR_RXT | ICR_RXO;
    _hwreg[ICS] = ICR_LSC;	// Force LSC IRQ. (For logging purposes only.)

    mac_set_link_up();

  }
};

PARAM(host82573, {
    HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
    unsigned found = 0;

    for (unsigned bdf, num = 0; (bdf = pci.search_device(0x2, 0x0, num++));) {
      unsigned cfg0 = pci.conf_read(bdf, 0x0);
      if ((cfg0 & 0xFFFF) != 0x8086 /* INTEL */) continue;

      // Find specific device
      unsigned i;
      for (i = 0; i < (sizeof(intel_nics)/sizeof(NICInfo)); i++) {
	if ((cfg0>>16 == intel_nics[i].devid) && (found++ == argv[0])) {
	  Host82573 *dev = new Host82573(argv[1], pci, mb.bus_hostop, mb.bus_acpi,
					 mb.clock(), bdf, intel_nics[i]);
	  mb.bus_hostirq.add(dev, &Host82573::receive_static<MessageIrq>);
	}
      }
    }
  },
  "host82573:instance,vnet - provide driver for Intel 82573L Ethernet controller.",
  "Example: 'host82573:0");

// EOF
