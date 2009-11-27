/**
 * Host Intel 82576 driver.
 *
 */

#include "vmm/motherboard.h"
#include "host/hostgenericata.h"
#include "host/hostpci.h"

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



// TODO Map registers.
// TODO Query extended capabilities.
// TODO MSI support

class Host82576 : public StaticReceiver<Host82576>
{
  enum MessageLevel {
    INFO  = 1<<0,
    DEBUG = 1<<1,

    ALL   = ~0U,
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

    // General
    RCTL      = 0x00100/4,	// RX Control
    RXPBS     = 0x02404/4,	// RX Packet Buffer Size
    
  };


  unsigned long _address;	 // Address of PCI config space
  volatile uint32_t *_hwreg;	 // Device MMIO registers (128K)
  uint16_t _hwioreg;		 // Device I/O  registers (32)

  unsigned _hostirq;
  const char *debug_getname() { return "Host82576"; }

  unsigned _msg_level;

public:

  bool receive(MessageIrq &msg)
  {
    if (msg.line != _hostirq || msg.type != MessageIrq::ASSERT_IRQ)  return false;

    // XXX Do something

    return true;
  };

  void msg(unsigned level, const char *msg, ...)
  {
    if ((level & _msg_level) != 0) {
      va_list ap;
      va_start(ap, msg);
      Logging::vprintf(msg, ap);
      va_end(ap);
    }
  }

  Host82576(HostPci &pci, DBus<MessageHostOp> &bus_hostop, Clock *clock, unsigned long address, unsigned hostirq)
    : _address(address), _hostirq(hostirq), _msg_level(ALL)
  {
    msg(INFO, "Found Intel 82576-style controller at %lx. Attaching IRQ %x.\n", address, hostirq);

    // Discover our register windows.
    _hwreg   = 0;
    _hwioreg = 0;

    for (unsigned bar_i = 0; bar_i < pci.MAX_BAR; bar_i++) {
      uint32_t bar_addr = address + pci.BAR0 + bar_i*pci.BAR_SIZE;
      uint32_t bar = pci.conf_read(bar_addr);
      size_t size  = pci.bar_size(bar_addr);

      if (bar == 0) continue;
      msg(DEBUG, "BAR %d: %08lx (size %08lx)\n", bar_i, bar, size);

      if ((bar & pci.BAR_IO) == 1) {
	// I/O bar
	if (size == 32) {
	  _hwioreg = bar & pci.BAR_IO_MASK;

	  // XXX Why <<8 ?
	  MessageHostOp iomsg(MessageHostOp::OP_ALLOC_IOIO_REGION, (_hwioreg << 8) | 5);
	  if (!bus_hostop.send(iomsg))
	    Logging::panic("%s could not allocate I/O window.\n", __PRETTY_FUNCTION__);

	  msg(INFO, "Found I/O window at %x.\n", _hwioreg);
	}
      } else {
	// Memory BAR
	// XXX 64-bit bars!
	uint32_t phys_addr = bar & pci.BAR_MEM_MASK;

	if (size == (1<<17 /* 128K */)) {
	  if (phys_addr & 0xFFF)
	    Logging::panic("%s: MMIO window not page-aligned.", __PRETTY_FUNCTION__);

	  MessageHostOp iomsg(MessageHostOp::OP_ALLOC_IOMEM_REGION, phys_addr | 17);
	  if (bus_hostop.send(iomsg) && iomsg.ptr)
	    _hwreg = reinterpret_cast<volatile uint32_t *>(iomsg.ptr);
	  else
	    Logging::panic("%s could not map register window.\n", __PRETTY_FUNCTION__);

	  msg(INFO, "Found MMIO window at %p (phys %lx).\n", _hwreg, phys_addr);
	}
      }

      if (!_hwreg && !_hwioreg)
	Logging::panic("%s could not find its register windows.\n", __PRETTY_FUNCTION__);

      // IRQ
      MessageHostOp irq_msg(MessageHostOp::OP_ATTACH_HOSTIRQ, hostirq);
      if (!(irq_msg.value == ~0U || bus_hostop.send(irq_msg)))
	Logging::panic("%s failed to attach hostirq %x\n", __PRETTY_FUNCTION__, hostirq);

    }

    // Capabilities
    // unsigned long sriov_cap = pci.find_extended_cap(_address, 0x160);
    // if (sriov_cap != 0)
    //   msg(INFO, "It is SR-IOV capable.\n");
  }
};

PARAM(host82576,
      {
	unsigned irqline;
	unsigned irqpin;
	HostPci pci(mb.bus_hwpcicfg);
	unsigned found = 0;

	for (unsigned address, num = 0; (address = pci.search_device(0x2, 0x0, num++, irqline, irqpin));) {
	  unsigned cfg0 = pci.conf_read(address + 0x0);
	  if (cfg0 == 0x10c98086) {
	    if (found++ == argv[0]) {
	      Host82576 *dev = new Host82576(pci, mb.bus_hostop, mb.clock(), address, argv[1]);
	      mb.bus_hostirq.add(dev, &Host82576::receive_static<MessageIrq>);
	    }
	  }
	}
      },
      "host82576:func,irq - provide driver for Intel 82576 Ethernet controller.");

// EOF
