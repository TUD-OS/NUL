// -*- Mode: C++ -*-
/** @file
 * Intel 82576 VF device model.
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <nul/types.h>
#include <nul/compiler.h>
#include <nul/motherboard.h>
#include <service/hexdump.h>
#include <service/time.h>
#include <service/net.h>
#include <service/endian.h>
#include <sys/syscalls.h>
#include <sys/semaphore.h>
#include <model/pci.h>
#include <nul/net.h>

#include "utils.h"

using namespace Endian;

// Status: IN FLUX
//
// TODO
// - handle BAR remapping
// - receive path does not set packet type in RX descriptor
// - TX legacy descriptors
// - interrupt thresholds
// - don't copy packet on TX path if offloading is not used
// - fancy offloads (SCTP CSO, IPsec, ...)
// - CSO support with TX legacy descriptors
// - scatter/gather support in MessageNetwork to avoid packet copy in
//   TX path.

class Model82576vf_vnet : public StaticReceiver<Model82576vf_vnet>
{
  EthernetAddr             _mac;
  Clock                    _clock;

#include "model/simplemem.h"

  static const mword _mmio_size = 0x4000; // 16KB
  static const mword _mmio_mask = ~(_mmio_size - 1);

  uint32 *_mmio;

  union {
    struct msix_table {
      uint64 msg_addr;
      uint32 msg_data;
      uint32 vector_control;
    } table[3];
    uint32 raw[3*4];
  } _msix;

  // Default guest-physical addresses for MMIO and MSI-X regs.
  const uint32 _mem_mmio;
  const uint32 _mem_msix;

  unsigned _bdf;

  // Caps
  unsigned _sem;

#include <model/82576vfpci.inc>

  // Software interface
  enum MBX {
    VF_RESET         = 0x0001U,
    VF_SET_MAC_ADDR  = 0x0002U,
    VF_SET_MULTICAST = 0x0003U,
    VF_SET_LPE       = 0x0005U,
    VF_SET_PROMISC   = 0x0006U,

    VF_SET_PROMISC_UNICAST = 0x04<<16,

    CMD_ACK          = 0x80000000U,
    CMD_NACK         = 0x40000000U,
    CTS              = 0x20000000U,
  };

  enum {
    VTCTRL   =      0/4,
    VTSTATUS =      8/4,
    VTEICR   = 0x1580/4,
    VTEIMS   = 0x1524/4,
    VTEIAC   = 0x152C/4,
    VTIVAR   = 0x1700/4,
    VTIVAR_MISC = 0x1740/4,
    VFMBX    = 0x800/4,
    VMMB     = 0xC40/4,

    // We use some special registers:

    // Set when an interrupt cause is newly raised. Set by either
    // switch or model, and cleared by the switch throttling logic.
    // Each bit is only meaningful, if the respective bit in VTEICR is
    // also set. Otherwise, it is automatically cleared.
    SHADOW_NEW_ICR = 0xF0/4,

    // Set for interrupts that must be injected NOW. This is written
    // by the switch after IRQ throttling and cleared by us.
    SHADOW_INJ_ICR = 0xF4/4,
  };

  // Or `ics` into the VTEICR register and schedule these
  // interrupts to be injected.
  void generate_irq(uint32 ics)
  {
   Cpu::atomic_or(&_mmio[VTEICR], ics);
   Cpu::atomic_or(&_mmio[SHADOW_NEW_ICR], ics);
  }

  /// Generate a mailbox/misc IRQ.
  void MISC_irq()
  {
    if ((_mmio[VTIVAR_MISC] & 0x80) && ((_mmio[VTIVAR_MISC] & 3) != 3))
      generate_irq(1U << (_mmio[VTIVAR_MISC] & 0x3));
  }

  void VMMB_cb(uint32 old, uint32 val)
  {
    // See 82576 datasheet Table 7-71 on page 357 for a good
    // explanation how this is supposed to work.

    // XXX Handle writes to VFU properly.
    if ((val & 1) != 0) {
      // Request for PF
      switch (_mmio[VFMBX] & 0xFFFF) {
      case VF_RESET:
	_mmio[VFMBX] |= CMD_ACK;
	_mmio[VFMBX + 1] = _mac.raw;
	_mmio[VFMBX + 2] = (_mac.raw >> 32) & 0xFFFF;
	Logging::printf("VF_RESET " MAC_FMT "\n", MAC_SPLIT(&_mac));
	break;
      case VF_SET_MAC_ADDR:
	_mmio[VFMBX] |= CMD_ACK;
	_mac.raw = static_cast<uint64>(_mmio[VFMBX + 2] & 0xFFFF) << 32 | _mmio[VFMBX + 1];
	Logging::printf("VF_SET_MAC " MAC_FMT "\n", MAC_SPLIT(&_mac));
	break;
      case VF_SET_MULTICAST: {
	uint8 count = (_mmio[VFMBX] >> 16) & 0xFF;
	Logging::printf("VF_SET_MULTICAST %08x (%u) %08x\n",
			_mmio[VFMBX], count, _mmio[VFMBX + 1]);
        Logging::printf(" ... but inexact filtering is not implemented!\n");
	_mmio[VFMBX] |= CMD_ACK | CTS;
      }
	break;
      default:
	Logging::printf("VF message unknown %08x\n", _mmio[VFMBX] & 0xFFFF);
	_mmio[VFMBX] |= CMD_NACK;
	// XXX
	break;
      }
      // Claim the buffer in a magic atomic way. If this were a real
      // VF, this would probably not happen at once.
      _mmio[VMMB] = (_mmio[VMMB] & ~(1<<2 /* VFU */)) | (1<<3 /* PFU */);
      // We have ACKed and wrote a response. Send an IRQ to inform the
      // VM.
      _mmio[VMMB] |= 1<<5 | 1<<4;     /* PFACK | PFSTS */
      MISC_irq();
    } else if ((val & 2) != 0) {
      // VF ACKs our message. Clear PFU to let the VM send a new
      // message.
      _mmio[VMMB] &= (1<<3 /* PFU */);
    }

    _mmio[VMMB] &= ~3;
  }

  void VTCTRL_cb(uint32 old, uint32 val)
  {
    if ((old ^ val) & (1<<26 /* Reset */)) {
      queue_init();
    }
  }

public:

  bool receive(MessagePciConfig &msg)
  {
    if (msg.bdf != _bdf) return false;

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

  // XXX Clean up!
  bool receive(MessageMem &msg)
  {
    // Memory decode disabled?
    if ((rPCISTSCTRL & 2) == 0) return false;

    if ((msg.phys & 0x3) != 0) {
      //Logging::printf("Misaligned MMIO access to %lx.\n", msg.phys);
      return false;
    }

    mword mmio_start = rPCIBAR0 & _mmio_mask;
    mword offset     = msg.phys - mmio_start;
    bool  in_mmio    = ((msg.phys & _mmio_mask) == mmio_start);

    if (msg.read) {
      if (in_mmio) {
	//Logging::printf("MMIO READ  %x\n", offset);
	*msg.ptr = register_read(offset);
        return true;
      } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
        *msg.ptr = MSIX_read(msg.phys - (rPCIBAR3 & ~0xFFF));
        return true;
      } else
        return false;
    } else /* !msg.read */ {
      if (in_mmio) {
	register_write(offset, *msg.ptr);
        return true;
      } else if ((msg.phys & ~0xFFF) == (rPCIBAR3 & ~0xFFF)) {
        MSIX_write(msg.phys - (rPCIBAR3 & ~0xFFF), *msg.ptr);
      } else return false;
    }

    return false;
  }

  bool receive(MessageMemRegion &msg)
  {
    switch ((msg.page) - (_mem_mmio >> 12)) {
    case 0x2:
    case 0x3:
      msg.ptr =  reinterpret_cast<char *>(_mmio + 0x2000/4);
      msg.start_page = msg.page & ~1;
      msg.count = 2;
      break;
    default:
      return false;
    }

    Logging::printf("82576VF MAP %lx+%x from %p\n", msg.start_page, msg.count, msg.ptr);
    return true;
  }

  bool receive(MessageVirtualNetPing &msg)
  {
    uint32 eicr = Cpu::xchg(&_mmio[0xF4/4], 0U);

    //Logging::printf("INJ %02x (now %02x)\n", eicr, _mmio[0xF4/4]);

    for (unsigned i = 0; i < 3; i++)
      if ((eicr & (1 << i)) and ((_msix.table[i].vector_control & 1) == 0)) {
	//Logging::printf("Generating MSI-X IRQ %d (%02x)\n", i, _msix.table[i].msg_data & 0xFF);
	MessageMem msg(false, _msix.table[i].msg_addr, &_msix.table[i].msg_data);
	_bus_mem->send(msg);
      }

    return true;
  }

  void queue_init()
  {
    // Disable queues
    _mmio[0x2828/4] = _mmio[0x3828/4] = 0;
    _mmio[0x2928/4] = _mmio[0x3928/4] = 0;
    
    // Mask all interrupts
    _mmio[VTEIMS]   = 0;
      
    memset(_mmio + 0x2000/4, 0, _mmio_size - 0x2000);

    // Enable RX0/TX0
    //_mmio[0x2828/4] = _mmio[0x3828/4] = (1 << 25);
      
    // SRRCTL
    _mmio[0x280C/4] =  4U<<8;
    _mmio[0x290C/4] = (4U<<8) | (1U<<31);
 
    _mmio[VTSTATUS] = 0x83;
    _mmio[VMMB]     = 0x80;

  }

  uint32 register_read(unsigned offset)
  {
    switch (offset/4) {
    case VTEICR: {
      uint32 val = _mmio[offset/4];
      Cpu::atomic_and(&_mmio[offset/4], ~val);
      return val;
    }
    case 0xC40/4: {		// VMMB
      uint32 val = _mmio[offset/4];
      Cpu::atomic_and(&_mmio[offset/4], ~(val & (11<<4)));
      return val;
    }
    case 0x1520/4:		// VTEICS
    case 0x1528/4:		// VTEIMC
      // Write only!
      return 0U;
    default:
      return _mmio[offset/4];
    }
  }
  
  void register_write(unsigned offset, uint32 val)
  {
    if (offset/4 == VTEIMS) {
      // This is in the critical path!
      Cpu::atomic_or(&_mmio[VTEIMS], val);
      //Logging::printf("VTEIMS %08x <- %08x\n", _mmio[0x1524/4], val);
      return;
    }

    uint32 old = _mmio[offset/4];

    switch (offset/4) {
    case VTCTRL:
      _mmio[VTCTRL] = val;
      VTCTRL_cb(old, val);
      break;
    case VTSTATUS:
    case 0x1048/4:		// VTFRTIMER
      // Read only!
      break;
    case VTEICR:
      // w1c
      Cpu::atomic_and(&_mmio[offset/4], ~val);
      break;
    case 0x1520/4:		// VTEICS
      // w1s
      generate_irq(val);
      break;
    case 0x1528/4:		// VTEIMC
      Cpu::atomic_and(&_mmio[0x1524/4], ~val);
      break;
    case VMMB:
      _mmio[offset/4] = val & ~3;
      VMMB_cb(old, val);
      break;
    default:
      _mmio[offset/4] = val;
      break;
    }
  }

  void device_reset()
  {
    PCI_init();
    rPCIBAR0 = _mem_mmio;
    rPCIBAR3 = _mem_msix;

    for (unsigned i = 0; i < 3; i++) {
      _msix.table[i].msg_addr = 0;
      _msix.table[i].msg_data = 0;
      _msix.table[i].vector_control = 1;
    }

    memset(_mmio, 0, _mmio_size);
    queue_init();
  }

  bool receive(MessageLegacy &msg)
  {
    if (msg.type == MessageLegacy::RESET)
      device_reset();

    return false;
  }

  Model82576vf_vnet(uint64 mac,
		    Clock *clock,
		    DBus<MessageHostOp> &hostop,
		    DBus<MessageVirtualNet> &vnet,
		    DBus<MessageMem> *bus_mem, DBus<MessageMemRegion> *bus_memregion,
		    uint32 mem_mmio, uint32 mem_msix, unsigned bdf)
    : _mac(mac), _clock(*clock), 
      _bus_memregion(bus_memregion), _bus_mem(bus_mem),
      _mem_mmio(mem_mmio), _mem_msix(mem_msix),
      _bdf(bdf)
  {
    Logging::printf("Attached 82576VF model at %08x+0x4000, %08x+0x1000\n",
		    mem_mmio, mem_msix);

    _mmio = reinterpret_cast<uint32 *>(alloc_from_guest(hostop, _mmio_size));
    device_reset();

    MessageVirtualNet vnetmsg(0, _mmio);
    if (not vnet.send(vnetmsg))
      Logging::panic("Could not attach to virtual network.\n");
  }

};

PARAM(82576vf_vnet,
      {
	MessageHostOp msg(MessageHostOp::OP_GET_MAC, 0UL);
	if (!mb.bus_hostop.send(msg)) Logging::panic("Could not get a MAC address");

	Model82576vf_vnet *dev = new Model82576vf_vnet(hton64(msg.mac) >> 16,
						       mb.clock(),
						       mb.bus_hostop, mb.bus_vnet,
						       &mb.bus_mem, &mb.bus_memregion,
						       (argv[1] == ~0UL) ? 0xF7CE0000 : argv[1],
						       (argv[2] == ~0UL) ? 0xF7CC0000 : argv[2],
						       PciHelper::find_free_bdf(mb.bus_pcicfg, ~0U));
	
	mb.bus_mem.add(dev, &Model82576vf_vnet::receive_static<MessageMem>);
	mb.bus_memregion.add(dev, &Model82576vf_vnet::receive_static<MessageMemRegion>);
	mb.bus_pcicfg.  add(dev, &Model82576vf_vnet::receive_static<MessagePciConfig>);
	mb.bus_legacy.  add(dev, &Model82576vf_vnet::receive_static<MessageLegacy>);
	mb.bus_vnetping.add(dev, &Model82576vf_vnet::receive_static<MessageVirtualNetPing>);

      },
      "82576vf_vnet:[promisc][,mem_mmio][,mem_msix] - attach an Intel 82576VF to the PCI bus.",
      "promisc   - if !=0, be always promiscuous (use for Linux VMs that need it for bridging) (Default 1)",
      // XXX promisc = 0 is ignored for now.
      "Example: 82576vf_vnet"
      );

// EOF
