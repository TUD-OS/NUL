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

class Host82576 : public StaticReceiver<Host82576>
{
  unsigned long _address;
  unsigned _hostirq;
  const char *debug_getname() { return "Host82576"; }

public:

  bool receive(MessageIrq &msg)
  {
    if (msg.line != _hostirq || msg.type != MessageIrq::ASSERT_IRQ)  return false;

    // XXX Do something

    return true;
  };

  Host82576(HostPci &pci, DBus<MessageHostOp> &bus_hostop, Clock *clock, unsigned long address, unsigned hostirq)
    : _address(address), _hostirq(hostirq)
  {
    
    MessageHostOp msg(MessageHostOp::OP_ATTACH_HOSTIRQ, hostirq);
    if (!(msg.value == ~0U || bus_hostop.send(msg)))
      Logging::panic("%s failed to attach hostirq %x\n", __PRETTY_FUNCTION__, hostirq);
  }
};

PARAM(host82576,
      {
	unsigned irqline; 
	unsigned irqpin;
	HostPci pci(mb.bus_hwpcicfg);

	for (unsigned address, num = 0; (address = pci.search_device(0x2, 0x0, num++, irqline, irqpin));) {
	  Logging::printf("Ethernet controller #%x at %x irq %x pin %x.\n", num, address, irqline, irqpin);

	  unsigned cfg0 = pci.conf_read(address + 0x0);
	  if (cfg0 == 0x10c98086) {
	    Logging::printf("Found Intel 82576-style controller.\n");
	    
	    Host82576 *dev = new Host82576(pci, mb.bus_hostop, mb.clock(), address, irqline);
	    mb.bus_hostirq.add(dev, &Host82576::receive_static<MessageIrq>);
	  } else {
	    Logging::printf("Ignored unknown Ethernet controller (%x:%x).\n",
			    cfg0 & 0xFFFF, cfg0 >> 16);
	  }

	  // HostAhci *dev = new HostAhci(pci, mb.bus_hostop, mb.bus_disk, mb.bus_diskcommit, mb.clock(), address, irqline);
	  // Logging::printf("DISK controller #%x AHCI at %x irq %x pin %x\n", num, address, irqline, irqpin);
	  // mb.bus_hostirq.add(dev, &HostAhci::receive_static<MessageIrq>);
	  // mb.bus_console.add(dev, &HostAhci::receive_static<MessageConsole>);

	  // MessageHostOp msg(MessageHostOp::OP_ATTACH_HOSTIRQ, irqline);
	  // if (!(msg.value == ~0U || mb.bus_hostop.send(msg)))
	  //   Logging::panic("%s failed to attach hostirq %x\n", __PRETTY_FUNCTION__, irqline);
	}
      },
      "host82576:irq=0x13 - provide a hostdriver for all Intel 82576 Ethernet controller.");

// EOF
