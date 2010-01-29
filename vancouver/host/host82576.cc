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
#include <host/host82576.h>
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

class VF82576 : public Base82576
{
  volatile uint32_t *_hwreg;

public:

  VF82576(HostPci &pci, DBus<MessageHostOp> &bus_hostop, Clock *clock, uint16_t bdf, uint64_t csr_addr, uint64_t msix_addr)
    : Base82576(clock, ALL, bdf) {
    msg(INFO, "VF: CSR %08llx MSIX %08llx\n", csr_addr, msix_addr);

    MessageHostOp iomsg(MessageHostOp::OP_ALLOC_IOMEM, csr_addr & ~0xFFF, 0x4000);
    if (bus_hostop.send(iomsg) && iomsg.ptr) {
      _hwreg = reinterpret_cast<uint32_t *>(iomsg.ptr);
      msg(VF, "CSR @ %p\n", iomsg.ptr);
    } else {
      msg(VF, "Mapping CSR memory failed.\n");
      return;
    }

    // XXX assert that PF is done
    //assert(_hwreg[VMB] & ... );

    msg(VF, "Status %08x VMB %08x Timer %08x\n", _hwreg[VSTATUS], _hwreg[VMB], _hwreg[VFRTIMER]);
    msg(VF, "Status %08x VMB %08x Timer %08x\n", _hwreg[VSTATUS], _hwreg[VMB], _hwreg[VFRTIMER]);

    // XXX Bad CopynPaste coding...
    msg(VF, "Trying to reset the VF...\n");
    _hwreg[VCTRL] = _hwreg[VCTRL] | CTRL_RST;
    spin(1000 /* 1ms */);
    if (!wait(_hwreg[VCTRL], CTRL_RST, 0))
      Logging::panic("%s: Reset failed.", __PRETTY_FUNCTION__);
    msg(VF, "... done\n");

    msg(VF, "Status %08x VMB %08x Timer %08x\n", _hwreg[VSTATUS], _hwreg[VMB], _hwreg[VFRTIMER]);
    msg(VF, "Status %08x VMB %08x Timer %08x\n", _hwreg[VSTATUS], _hwreg[VMB], _hwreg[VFRTIMER]);
  }
};

class Host82576 : public Base82576, public StaticReceiver<Host82576>
{
private:
  DBus<MessageHostOp> &_bus_hostop;
  volatile uint32_t *_hwreg;     // Device MMIO registers (128K)

  unsigned _hostirq;
  const char *debug_getname() { return "Host82576"; }

  EthernetAddr _mac;

  // MSI-X
  unsigned _msix_table_size;

  struct msix_table {
    volatile uint64_t msg_addr;
    volatile uint32_t msg_data;
    volatile uint32_t vector_control;
  } *_msix_table;

  volatile uint64_t *_msix_pba;

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
    msg(INFO, "%4s %sBASE-T %cD | %u VFs %s | %4d RX | %4d TX\n", up,
	speed[(status & STATUS_SPEED) >> STATUS_SPEED_SHIFT],
	status & STATUS_FD ? 'F' : 'H',
	(status & STATUS_NUMVF) >> STATUS_NUMVF_SHIFT,
	status & STATUS_IOV ? "ON" : "OFF",
	_hwreg[GPRC],
	_hwreg[GPTC]);
  }

  unsigned enabled_vfs()
  {
    return (_hwreg[STATUS] & STATUS_NUMVF) >> STATUS_NUMVF_SHIFT;
  }

  void handle_vf_reset(unsigned vf_no)
  {
    msg(VF, "FLR on VF%d.\n", vf_no);
  }

  enum MBX {
    VF_RESET        = 0x0001U,
    VF_SET_MAC_ADDR = 0x0002U,
    PF_CONTROL_MSG  = 0x0100U,

    ACK             = 0x80000000U,
    NACK            = 0x40000000U,
    CTS             = 0x20000000U,
  };

  void handle_vf_message(unsigned vf_no)
  {
    unsigned pfmbx    = PFMB0 + vf_no;
    unsigned pfmbxmem = PFMBMEM + vf_no*0x10;
    unsigned mem0     = _hwreg[pfmbxmem];
    
    msg(VF, "Message from VF%d: PFMBX %08x PFMBXMEM[0] %08x.\n", vf_no,
	_hwreg[pfmbx], _hwreg[pfmbxmem]);

    switch (mem0) {
    case VF_RESET: {
      union {
	char mac_addr[6];
	uint32_t raw[2];
      };
      raw[2] = 0;
      // XXX Make this configurable!
      mac_addr = { 0xa6, 0xd9, 0xed, 0x36, 0x44, vf_no }; 
      _hwreg[pfmbxmem] = mem0 | ACK;
      _hwreg[pfmbxmem + 1] = raw[0];
      _hwreg[pfmbxmem + 2] = raw[1];
    }
    default:
      _hwreg[pfmbxmem] = mem0 | NACK;
    };

    _hwreg[pfmbx] = Sts | Ack;
  }

  void handle_vf_ack(unsigned vf_no)
  {
    msg(VF, "Message ACK from VF%d.\n", vf_no);
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

    uint32_t icr  = _hwreg[ICR];
    uint32_t eicr = _hwreg[EICR];
    msg(IRQ, "IRQ! ICR = %08x EICR = %08x\n", icr, eicr);
    if ((eicr|icr) == 0) return false;

    if (icr & IRQ_LSC) {
      bool gone_up = (_hwreg[STATUS] & STATUS_LU) != 0;
      msg(IRQ, "Link status changed to %s.\n", gone_up ? "UP" : "DOWN");
      if (gone_up) {
	_hwreg[RCTL] |= RCTL_RXEN | RCTL_BAM;
	_hwreg[TCTL] |= TCTL_TXEN;
	//_rx->enable();
	msg(TX | RX | IRQ, "RX/TX enabled.\n");
      } else {
	// Down
	msg(TX | RX | IRQ, "RX/TX disabled.\n");
	_hwreg[RCTL] &= ~RCTL_RXEN;
	_hwreg[TCTL] &= ~TCTL_TXEN;
      }

      log_device_status();
    }

    if (icr & IRQ_VMMB) {
      uint32_t vflre  = _hwreg[VFLRE];
      uint32_t mbvficr = _hwreg[MBVFICR];
      uint32_t mbvfimr = _hwreg[MBVFIMR];
      msg(VF, "VMMB: VFLRE %08x MBVFICR %08x MBVFIMR %08x\n", vflre, mbvficr, mbvfimr);

      // Check FLRs
      for (uint32_t mask = 1, cur = 0; cur < 8; cur++, mask<<=1)
	if (vflre & mask)
	  handle_vf_reset(cur);

      // Check incoming
      for (uint32_t mask = 1, cur = 0; cur < 8; cur++, mask<<=1) {
	if (mbvficr & mask)
	  handle_vf_message(cur);
	if (mbvficr & (mask << 16)) // Check ACKs
	  handle_vf_ack(cur);
      }

      // Clear bits
      _hwreg[VFLRE]  = vflre;
      _hwreg[MBVFICR] = mbvficr;
    }


    return true;
  };

  Host82576(HostPci pci, DBus<MessageHostOp> &bus_hostop, Clock *clock,
	    unsigned bdf, unsigned hostirq)
    : Base82576(clock, ALL, bdf), _bus_hostop(bus_hostop), _hostirq(hostirq)
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

    // VT Setup
    _hwreg[QDE] |= 0xFFFF;	// Enable packet dropping for all 16
				// queues.
    // Disable default pool. Uninteresting packets will be
    // dropped. Also enable replication to allow VF-to-VF
    // communication.
    _hwreg[VT_CTL] |= VT_CTL_DIS_DEF_POOL | VT_CTL_REP_ENABLE;

    // Disable RX and TX for all VFs.
    _hwreg[VFRE] = 0;
    _hwreg[VFTE] = 0;

    // Allow all VFs to send IRQs to us.
    _hwreg[MBVFIMR] = 0xFF;

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
    
    // XXX Enabling the PBA bit without MULTIPLE_MSIX seems to freeze
    //     the whole box. The datasheet implies that this is a poor
    //     choice anyway...
    //_hwreg[GPIE] = (_hwreg[GPIE] & ~GPIE_MULTIPLE_MSIX) | GPIE_PBA;

    // Disable MULTIPLE_MSIX feature. We use just one.
    _hwreg[GPIE] = _hwreg[GPIE] & ~GPIE_MULTIPLE_MSIX;

    pci.conf_write(bdf, msix_cap, pci.conf_read(bdf, msix_cap) | MSIX_ENABLE);
    _msix_table[0].msg_addr = 0xFEE00000 | 0<<12 /* CPU */ /* DH/RM */;
    _msix_table[0].msg_data = hostirq + 0x20;
    _msix_table[0].vector_control &= ~1; // Preserve content as per spec 6.8.2.9
    msg(IRQ, "MSI-X setup complete.\n");

    // Wait for Link Up event
    //_hwreg[IMS] = IRQ_LSC | IRQ_FER | IRQ_NFER | IRQ_RXDW | IRQ_RXO | IRQ_TXDW;
    _hwreg[IMS] = ~0U;

    // PF Setup complete
    _hwreg[CTRL_EXT] |= CTRL_EXT_PFRSTD;
    msg(INFO, "Initialization complete.\n");

    // VFs
    // unsigned sriov_cap = pci.find_extended_cap(bdf, HostPci::EXTCAP_SRIOV);
    // assert(sriov_cap != 0);
    
    // unsigned vf_offset = pci.conf_read(bdf, sriov_cap + 0x14);
    // unsigned vf_stride = vf_offset >> 16;
    // vf_offset &= 0xFFFF;

    // for (unsigned cur_vf = 0; cur_vf < enabled_vfs(); cur_vf++) {
    //   uint16_t vf_bdf = bdf + vf_offset + vf_stride*cur_vf;

    //   assert(pci.vf_bar_size(bdf, 0) == 0x4000);
    //   uint64_t bar0_base = pci.vf_bar_base(bdf, 0) + pci.vf_bar_size(bdf, 0)*cur_vf;
    //   uint64_t bar3_base = pci.vf_bar_base(bdf, 3) + pci.vf_bar_size(bdf, 3)*cur_vf;

    //   // Try to drive one VF
    //   if (cur_vf == 0)
    // 	new VF82576(pci, bus_hostop, _clock, vf_bdf, bar0_base, bar3_base);
    // }

  }
};

PARAM(host82576, {
    HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
    unsigned found = 0;

    for (unsigned bdf, num = 0; (bdf = pci.search_device(0x2, 0x0, num++));) {
      unsigned cfg0 = pci.conf_read(bdf, 0x0);
      if (cfg0 == 0x10c98086) {
	if (found++ == argv[0]) {
	  MessageHostOp msg1(MessageHostOp::OP_ASSIGN_PCI, bdf);
	  bool dmar = mb.bus_hostop.send(msg1);
          if (!dmar) {
            Logging::printf("Could not assign PCI device. Skipping.\n");
            continue;
          }

	  Host82576 *dev = new Host82576(pci, mb.bus_hostop, mb.clock(), bdf,
					 // XXX Does not work reliably! (ioapic assertion) 
					 //pci.get_gsi(bdf, argv[1])
					 argv[1]
					 );
	  mb.bus_hostirq.add(dev, &Host82576::receive_static<MessageIrq>);
	}
      }
    }
  },
  "host82576:func,irq - provide driver for Intel 82576 Ethernet controller.");

// EOF
