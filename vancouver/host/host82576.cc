/**
 * Host Intel 82576 driver.
 *
 * This device usually comes as a PCIe card with two functions, each
 * acting as a separate NIC with a few exceptions. There are several
 * shared resources: EEPROM, PHY(s), Flash. If access to those
 * resources is desired, the hardware-semaphore of the 82576 has to be
 * used. We can blissfully ignore it, because we do not need access to
 * any shared resource.
 */

#include <vmm/motherboard.h>
#include <host/hostgenericata.h>
#include <host/hostpci.h>
#include <cstdint>

// lspci -v:
// 02:00.0 Ethernet controller: Intel Corporation Device 10c9 (rev 01)
// 	Subsystem: Intel Corporation Device a03c
// 	Flags: bus master, fast devsel, latency 0, IRQ 16
// 	Memory at 90820000 (32-bit, non-prefetchable) [size=128K]
// 	I/O ports at 2020 [size=32]
// 	Memory at 90844000 (32-bit, non-prefetchable) [size=16K]
// 	Expansion ROM at 91000000 [disabled] [size=4M]
// 	Capabilities: [40] Power Management version 3
// 	Capabilities: [50] Message Signalled Interrupts: Mask+ 64bit+ Count=1/1 Enable-
// 	Capabilities: [70] MSI-X: Enable+ Mask- TabSize=10
// 	Capabilities: [a0] Express Endpoint, MSI 00
// 	Capabilities: [100] Advanced Error Reporting
// 		UESta:	DLP- SDES- TLP- FCP- CmpltTO- CmpltAbrt- UnxCmplt- RxOF- MalfTLP- ECRC- UnsupReq- ACSVoil-
// 		UEMsk:	DLP- SDES- TLP- FCP- CmpltTO- CmpltAbrt- UnxCmplt- RxOF- MalfTLP- ECRC- UnsupReq- ACSVoil-
// 		UESvrt:	DLP+ SDES- TLP- FCP+ CmpltTO- CmpltAbrt- UnxCmplt- RxOF+ MalfTLP+ ECRC- UnsupReq- ACSVoil-
// 		CESta:	RxErr+ BadTLP- BadDLLP- Rollover- Timeout- NonFatalErr+
// 		CESta:	RxErr- BadTLP- BadDLLP- Rollover- Timeout- NonFatalErr+
// 		AERCap:	First Error Pointer: 00, GenCap- CGenEn- ChkCap- ChkEn-
// 	Capabilities: [140] Device Serial Number e8-e2-4d-ff-ff-21-1b-00
// 	Capabilities: [150] Alternative Routing-ID Interpretation (ARI)
// 		ARICap:	MFVC- ACS-, Next Function: 1
// 		ARICtl:	MFVC- ACS-, Function Group: 0
// 	Capabilities: [160] Single Root I/O Virtualization (SR-IOV)
// 		IOVCap:	Migration-, Interrupt Message Number: 000
// 		IOVCtl:	Enable- Migration- Interrupt- MSE- ARIHierarchy-
// 		IOVSta:	Migration-
// 		Initial VFs: 8, Total VFs: 8, Number of VFs: 8, Function Dependency Link: 00
// 		VF offset: 384, stride: 2, Device ID: 10ca
// 		Supported Page Size: 00000553, System Page Size: 00000001
// 		VF Migration: offset: 00000000, BIR: 1
// 	Kernel driver in use: igb
// 	Kernel modules: igb

// TODO Query extended capabilities.
// TODO MSI support

// Our test NICs:
// 00:1b:21:4d:e2:e8
// 00:1b:21:4d:e2:e9

#define ALIGN(x) __attribute__((aligned(x)))

struct EthernetAddr {
  union {
    uint64_t raw;
    uint8_t byte[6];
  };
};

#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_SPLIT(x) (x)->byte[0], (x)->byte[1], (x)->byte[2],(x)->byte[3], (x)->byte[4], (x)->byte[5]

void hexdump(const char *data, size_t len)
{
  const unsigned chars_per_row = 16;
  const char *data_end = data + len;
  size_t cur = 0;
  while (cur < len) {
    Logging::printf("%08x:", cur);
    for (unsigned i = 0; i < chars_per_row; i++)
      if (data+i < data_end) {
	Logging::printf(" %02x", ((const unsigned char *)data)[i]);
      } else {
	Logging::printf("   ");
      }
    Logging::printf(" | ");
    for (unsigned i = 0; i < chars_per_row; i++) {
      if (data < data_end) {
	Logging::printf("%c", ((data[0] >= 32) && (data[0] > 0)) ? data[0] : '.');
      } else {
	Logging::printf(" ");
      }
      data++;
    }
    Logging::printf("\n");
    cur += chars_per_row;
  }
}

class Host82576 : public StaticReceiver<Host82576>
{

  // Messages are tagged with one or more constants from this
  // bitfield. You can disable certain kinds of messages in the
  // constructor.
  enum MessageLevel {
    INFO  = 1<<0,
    DEBUG = 1<<1,
    PCI   = 1<<2,
    IRQ   = 1<<3,
    RX    = 1<<4,

    ALL   = ~0U,
  };

  enum Spec {
    RX_QUEUES = 16,
  };

  enum PCI {
    MSIX_ENABLE = (1UL << 31),
  };
  
  enum Register {
    // Spec p.399

    // General
    CTRL      = 0x0000/4,
    STATUS    = 0x0008/4,
    CTRL_EXT  = 0x0018/4,
    MDIC      = 0x0020/4,
    SERDESCTL = 0x0024/4,

    FRTIMER   = 0x1048/4,	// Free Running Timer

    // Interrupts
    ICR       = 0x01500/4,
    ICS       = 0x01504/4,
    IMS       = 0x01508/4,	// Interrupt Mask Set/Read
    IMC       = 0x0150C/4,	// Interrupt Mask Clear

    EIMS      = 0x01528/4,	// Extended Interrupt Mask Clear

    // General
    RCTL      = 0x00100/4,	// RX Control
    RXPBS     = 0x02404/4,	// RX Packet Buffer Size

    RAL0      = 0x05400/4,
    RAH0      = 0x05404/4,

  };

  enum Ctrl {
    CTRL_GIO_MASTER_DISABLE = 1<<2,
    CTRL_SLU      = 1<<6,	// Set Link Up
    CTRL_FRCSPD   = 1<<11,	// Force Speed
    CTRL_FRCDPLX  = 1<<12,	// Force Duplex
    CTRL_RST      = 1<<26,	// Device Reset
    CTRL_VME      = 1<<30,	// VLAN Mode Enable
  };

  enum CtrlExt {
    CTRL_EXT_LINK_MODE = 0x3<<22, // Set to 0 for Copper link.
  };

  enum Status {
    STATUS_FD     = 0x1,        // Full duplex (0 = Half Duplex, 1 = Full Duplex)
    STATUS_LU     = 0x2,        // Link Up
    STATUS_LAN_ID = 0x3<<2,     // LAN ID (2-bit)
    STATUS_SPEED  = 0x3<<6,
    STATUS_SPEED_SHIFT = 6,

    STATUS_NUMVF  = 0xF<<14,    // Number of VFs
    STATUS_NUMVF_SHIFT = 14,
    STATUS_IOV    = 1<<18,      // Value of VF Enable Bit
    STATUS_GIO_MASTER_ENABLE = 1<<19,
  };

  enum StatusSpeed {
    SPEED_10M     = 0x0,
    SPEED_100M    = 0x1,
    SPEED_1000M   = 0x2,
    SPEED_1000M2  = 0x3,
  };

  enum Interrupt {
    IRQ_TXDW      = 1<<7,	// Transmit Descriptor Writeback
    IRQ_LSC       = 1<<2,       // Link Status Change
    IRQ_RXO       = 1<<6,	// Receiver Overrun
    IRQ_RXDW      = 1<<7,	// Receive Descriptor Writeback
    IRQ_VMMB      = 1<<8,	// VM Mailbox/FLR
    IRQ_FER       = 1<<22,      // Fatal Error
    IRQ_NFER      = 1<<23,      // Non-Fatal Error
    IRQ_SWD       = 1<<26,      // Software Watchdog expired
    IRQ_INTA      = 1U<<31,     // INTA asserted (not available for MSIs)
  };

  enum ReceiveControl {
    RCTL_RXEN     = 1<<1,	// Receive Enable
    RCTL_SBP      = 1<<2,	// Store Bad Packets
    RCTL_UPE      = 1<<3,	// Unicast Promiscuous
    RCTL_MPE      = 1<<4,	// Multicast Promiscuous
    RCTL_LPE      = 1<<5,	// Long Packet Enable
    RCTL_BAM      = 1<<15,	// Broadcast Accept Mode
    RCTL_BSIZE    = 3<<16,	// Buffer Size (default: 0 = 2K)
  };

  enum PacketType {
    TYPE_L2       = 1<<11,	// L2 packet
    TYPE_IPV4     = 1,
    TYPE_IPV4E    = 1<<1,
    TYPE_IPV6     = 1<<2,
    TYPE_IPV6E    = 1<<3,
    TYPE_TCP      = 1<<4,
    TYPE_UDP      = 1<<5,

  };

  Clock *_clock;
  DBus<MessageHostOp> &_bus_hostop;
  DBus<MessageNetwork> &_bus_network;
  unsigned long _bdf;
  volatile uint32_t *_hwreg;     // Device MMIO registers (128K)

  unsigned _hostirq;
  const char *debug_getname() { return "Host82576"; }

  unsigned _msg_level;
  EthernetAddr _mac;

  // MSI-X
  unsigned _msix_table_size;

  struct msix_table {
    volatile uint64_t msg_addr;
    volatile uint32_t msg_data;
    volatile uint32_t vector_control;
  } *_msix_table;

  volatile uint64_t *_msix_pba;

  __attribute__ ((format (printf, 3, 4)))
  void msg(unsigned level, const char *msg, ...)
  {
    if ((level & _msg_level) != 0) {
      va_list ap;
      va_start(ap, msg);
      Logging::printf("82576 %lx: ", _bdf & 0xF);
      Logging::vprintf(msg, ap);
      va_end(ap);
    }
  }

  uintptr_t to_phys(void *ptr) {
    MessageHostOp msg(MessageHostOp::OP_VIRT_TO_PHYS, reinterpret_cast<unsigned long>(ptr));
    if (_bus_hostop.send(msg) && msg.phys) {
      return msg.phys;
    } else Logging::panic("Could not resolve %p's physical address.\n", ptr);
  }

  void log_device_status()
  {
    uint32_t status = _hwreg[STATUS];
    const char *up = (status & STATUS_LU ? "UP" : "DOWN");
    const char *speed[] = { "10", "100", "1000", "1000" };
    msg(INFO, "%s %sBASE-T %cD | %u VFs %s\n", up,
	speed[(status & STATUS_SPEED) >> STATUS_SPEED_SHIFT],
	status & STATUS_FD ? 'F' : 'H',
	(status & STATUS_NUMVF) >> STATUS_NUMVF_SHIFT,
	status & STATUS_IOV ? "ON" : "OFF");
  }

  void spin(unsigned micros)
  {
    timevalue done = _clock->abstime(micros, 1000000);
    while (_clock->time() < done)
      Cpu::pause();
  }

  bool wait(volatile uint32_t &reg, uint32_t mask, uint32_t value,
	    unsigned timeout_micros = 1000000 /* 1s */)
  {
    timevalue timeout = _clock->abstime(timeout_micros, 1000000);

    while ((reg & mask) != value) {
      Cpu::pause();
      if (_clock->time() >= timeout)
	return false;
    }
    return true;
  }

public:

  EthernetAddr ethernet_address()
  {
    // Hardware initializes the Receive Address registers of queue 0
    // with the MAC address stored in the EEPROM.
    EthernetAddr a = {{ _hwreg[RAL0] | (((uint64_t)_hwreg[RAH0] & 0xFFFF) << 32) }};

    if ((_bdf & 0xF) == 1)
      a.byte[5] ^= 1;

    return a;
  }

  bool receive(MessageIrq &irq_msg)
  {
    if (irq_msg.line != _hostirq || irq_msg.type != MessageIrq::ASSERT_IRQ)  return false;

    uint32_t irq_cause = _hwreg[ICR];
    msg(IRQ, "IRQ! ICR = %x\n", irq_cause);
    if (irq_cause == 0) return false;

    if (irq_cause & IRQ_LSC) {
      bool gone_up = (_hwreg[STATUS] & STATUS_LU) != 0;
      msg(IRQ, "Link status changed to %s.\n", gone_up ? "UP" : "DOWN");
      if (gone_up) {
	_hwreg[RCTL] = RCTL_RXEN | RCTL_BAM;
	//_rx->enable();
	msg(RX | IRQ, "RX enabled.\n");
      } else {
	// Down
	msg(RX | IRQ, "RX disabled.\n");
	_hwreg[RCTL] &= ~RCTL_RXEN;
      }

      log_device_status();
    }


    return true;
  };

  Host82576(HostPci &pci, DBus<MessageHostOp> &bus_hostop, DBus<MessageNetwork> &bus_network, 
	    Clock *clock, unsigned bdf, unsigned hostirq)
    : _clock(clock), _bus_hostop(bus_hostop), _bus_network(bus_network),
      _bdf(bdf), _hostirq(hostirq), _msg_level(ALL)
  {
    msg(INFO, "Found Intel 82576-style controller. Attaching IRQ %u.\n", _hostirq);

    // MSI-X preparations
    unsigned msix_cap = pci.find_cap(bdf, HostPci::CAP_MSIX);
    assert(msix_cap != 0);
    unsigned msix_table = pci.conf_read(bdf, msix_cap + 4);
    unsigned msix_pba   = pci.conf_read(bdf, msix_cap + 8);

    
    msg(PCI, "MSI-X[0] = %08x\n", pci.conf_read(bdf, msix_cap));
    _msix_table_size = ((pci.conf_read(bdf, msix_cap)>>16) & ((1<<11)-1))+1;
    _msix_table = NULL;
    _msix_pba = NULL;

    // Scan BARs and discover our register windows.
    _hwreg   = 0;

    for (unsigned bar_i = 0; bar_i < pci.MAX_BAR; bar_i++) {
      uint32_t bar_addr = pci.BAR0 + bar_i*pci.BAR_SIZE;
      uint32_t bar = pci.conf_read(_bdf, bar_addr);
      size_t size  = pci.bar_size(_bdf, bar_addr);

      if (bar == 0) continue;
      msg(PCI, "BAR %u: %08x (size %08zx)\n", bar_i, bar, size);

      if ((bar & pci.BAR_IO) != 1) {
	// Memory BAR
	// XXX 64-bit bars!
	uint32_t phys_addr = bar & pci.BAR_MEM_MASK;

	
	MessageHostOp iomsg(MessageHostOp::OP_ALLOC_IOMEM, phys_addr & ~0xFFF, size);
	if (bus_hostop.send(iomsg) && iomsg.ptr) {

	  if (size == (1<<17 /* 128K */)) {
	    _hwreg = reinterpret_cast<volatile uint32_t *>(iomsg.ptr);
	    msg(INFO, "Found MMIO window at %p (phys %x).\n", _hwreg, phys_addr);
	  }

	  if (bar_i == (msix_table & 7)) {
	    _msix_table = reinterpret_cast<struct msix_table *>(iomsg.ptr + (msix_table&~0x7));
	    msg(INFO, "MSI-X Table at %p (phys %x, elements %x).\n", _msix_table, phys_addr, _msix_table_size);
	  }
	  
	  if (bar_i == (msix_pba & 7)) {
	    _msix_pba = reinterpret_cast<volatile uint64_t *>(iomsg.ptr + (msix_pba&~0x7));
	    msg(INFO, "MSI-X PBA   at %p (phys %x, elements %x).\n", _msix_pba, phys_addr, _msix_table_size);
	  }

	  } else {
	    Logging::panic("%s could not map BAR %u.\n", __PRETTY_FUNCTION__, bar);
	  }
      }
    }

    if ((_hwreg == 0)) Logging::panic("Could not find 82576 register windows.\n");
    if (!(_msix_table || _msix_pba)) Logging::panic("MSI-X initialization failed.\n");

    /// Initialization (4.5)
    msg(INFO, "Perform Global Reset.\n");
    // Disable interrupts
    _hwreg[IMS] = 0;
    _hwreg[EIMS] = 0;

    // Global Reset (5.2.3.3)
    _hwreg[CTRL] = _hwreg[CTRL] | CTRL_GIO_MASTER_DISABLE;
    if (!wait(_hwreg[STATUS], STATUS_GIO_MASTER_ENABLE, 0))
      Logging::panic("%s: Device hang?", __PRETTY_FUNCTION__);

    _hwreg[CTRL] = _hwreg[CTRL] | CTRL_RST;
    spin(1000 /* 1ms */);
    if (!wait(_hwreg[CTRL], CTRL_RST, 0))
      Logging::panic("%s: Reset failed.", __PRETTY_FUNCTION__);

    // Disable Interrupts (again)
    _hwreg[IMS] = 0;
    _hwreg[EIMS] = 0;
    msg(INFO, "Global Reset successful.\n");

    // XXX Do we need to do anything for flow control?

    // Direct Copper link mode (set link mode to 0)
    _hwreg[CTRL_EXT] &= ~CTRL_EXT_LINK_MODE;

    // Enable PHY autonegotiation (4.5.7.2.1)
    _hwreg[CTRL] = (_hwreg[CTRL] & ~(CTRL_FRCSPD | CTRL_FRCDPLX)) | CTRL_SLU;

    // XXX Set CTRL.RFCE/CTRL.TFCE

    _mac = ethernet_address();
    msg(INFO, "We are " MAC_FMT "\n", MAC_SPLIT(&_mac));

    // We would setup RX queues here, if we'd use any.
    //_rx = new RXQueue(this, 0);

    // IRQ
    MessageHostOp irq_msg(MessageHostOp::OP_ATTACH_HOSTIRQ, hostirq);
    if (!(irq_msg.value == ~0U || bus_hostop.send(irq_msg)))
      Logging::panic("%s failed to attach hostirq %x\n", __PRETTY_FUNCTION__, hostirq);
    msg(IRQ, "Attached to IRQ %u. Waiting for link to come up.\n", hostirq);

    // Configuring one MSI-X vector
    pci.conf_write(bdf, msix_cap, pci.conf_read(bdf, msix_cap) | MSIX_ENABLE);
    _msix_table[0].msg_addr = 0xFEE00000 | 0<<12 /* CPU */ /* DH/RM */;
    _msix_table[0].msg_data = hostirq + 0x20;
    _msix_table[0].vector_control |= 1; // Preserve content as per spec 6.8.2.9

    // Wait for Link Up event
    //_hwreg[IMS] = IRQ_LSC | IRQ_FER | IRQ_NFER | IRQ_RXDW | IRQ_RXO | IRQ_TXDW;
    _hwreg[IMS] = ~0U;

    _hwreg[ICS] = IRQ_LSC;

    // Capabilities
    // unsigned long sriov_cap = pci.find_extended_cap(_address, 0x160);
    // if (sriov_cap != 0)
    //   msg(INFO, "It is SR-IOV capable.\n");
  }
};

PARAM(host82576, {
    unsigned irqline;
    unsigned irqpin;
    HostPci pci(mb.bus_hwpcicfg);
    unsigned found = 0;

    for (unsigned bdf, num = 0; (bdf = pci.search_device(0x2, 0x0, num++, irqline, irqpin));) {
      unsigned cfg0 = pci.conf_read(bdf, 0x0);
      if (cfg0 == 0x10c98086) {
	if (found++ == argv[0]) {
	  MessageHostOp msg1(MessageHostOp::OP_ASSIGN_PCI, bdf);
	  bool dmar = mb.bus_hostop.send(msg1);
          if (!dmar) {
            Logging::printf("Could not assign PCI device. Skipping.\n");
            continue;
          }

	  Host82576 *dev = new Host82576(pci, mb.bus_hostop, mb.bus_network,
					 mb.clock(), bdf, argv[1]);
	  mb.bus_hostirq.add(dev, &Host82576::receive_static<MessageIrq>);
	}
      }
    }
  },
  "host82576:func,irq - provide driver for Intel 82576 Ethernet controller.");

// EOF
