/**
 * AHCI emulation.
 *
 * Copyright (C) 2008-2009, Bernhard Kauer <bk@vmmon.org>
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

#include "vmm/motherboard.h"
#include "models/sata.h"
#include "models/pci.h"


#define check2(X) { unsigned __res = X; if (__res) return __res; }

class ParentIrqProvider
{
 public:
  virtual void trigger_irq (void * child) = 0;
};

/**
 * A port of an AhciController.
 *
 * State: unstable
 * Features: register set, FIS
 * Missing: plenty
 */
class AhciPort : public HwRegisterSet<AhciPort>, public FisReceiver
{
  DBus<MessageMemWrite> *_bus_memwrite;
  DBus<MessageMemRead> *_bus_memread;
  FisReceiver *_drive;
  ParentIrqProvider *_parent;
  int _reg_tfd;
  int _reg_cmd;
  int _reg_ci;
  int _reg_sact;
  unsigned _ccs;
  unsigned _inprogress;
  bool _need_initial_fis;

  /**
   * Copy to the user.
   */
  bool copy_out(unsigned long address, void *ptr, unsigned count)
  {
    MessageMemWrite msg(address, ptr, count);
    if (!_bus_memwrite->send(msg))
      // XXX DMA bus abort
      Logging::panic("%s() could not copy out %x bytes to %lx\n", __PRETTY_FUNCTION__, count, address);
    return true;
  }

  /**
   * Copy from the user.
   */
  bool copy_in(unsigned long address, void *ptr, unsigned count)
  {
    MessageMemRead msg(address, ptr, count);
    if (!_bus_memread->send(msg))
      // XXX DMA bus abort
      Logging::panic("%s() could not copy in %x bytes from %lx\n", __PRETTY_FUNCTION__, count, address);
    return true;
  }

 public:

  enum {
    CALLBACK_CI = 1,
    CALLBACK_CMD,
    CALLBACK_SCTL,
    CALLBACK_IS,
  };

  void set_parent(ParentIrqProvider *parent, DBus<MessageMemWrite> *bus_memwrite, DBus<MessageMemRead> *bus_memread)
  {
    _parent = parent;
    _bus_memwrite = bus_memwrite;
    _bus_memread = bus_memread;
  }


  /**
   * Receive a FIS from the Device.
   */
  void receive_fis(unsigned fislen, unsigned *fis)
  {
    unsigned copy_offset;
    unsigned cmd = 0;

    HwRegisterSet<AhciPort>::read_reg(_reg_cmd, cmd);

    unsigned fisbase = 0;
    HwRegisterSet<AhciPort>::read_reg(HwRegisterSet<AhciPort>::find_reg("PxFBU"), fisbase);


    // fis receiving enabled?
    // XXX bug in 2.6.27?
    //if (!_need_initial_fis && ~cmd & 0x10) { Logging::printf("skip FIS %x\n", fis[0]); return; }

    // update status and error fields
    HwRegisterSet<AhciPort>::modify_reg(_reg_tfd, 0xffff, fis[0] >> 16);

    switch (fis[0] & 0xff)
      {
      case 0x34: // d2h register fis
	assert(fislen == 5);
	copy_offset = 0x40;

	if (_need_initial_fis)
	  {
	    // update signature
	    HwRegisterSet<AhciPort>::modify_reg(HwRegisterSet<AhciPort>::find_reg("PxSIG"), 0xffffffff, fis[1] << 8 | fis[3] & 0xff);
	    // set PxSTSS since a device is available
	    HwRegisterSet<AhciPort>::modify_reg(HwRegisterSet<AhciPort>::find_reg("PxSSTS"), 0xfff, 0x113);
	    Logging::printf("initial fis received %x %x\n", fis[1], fis[3]);
	    _need_initial_fis = false;
	  }

	// we finished the current command
	if (~fis[0] & 0x80000 && fis[4])  {
	  unsigned mask = 1 << (fis[4] - 1);
	  if (mask & ~_inprogress)
	    Logging::panic("XXX broken %x,%x inprogress %x\n", fis[0], fis[4], _inprogress);
	  _inprogress &= ~mask;
	  HwRegisterSet<AhciPort>::modify_reg(_reg_ci, mask, 0);
	  HwRegisterSet<AhciPort>::modify_reg(_reg_sact, mask, 0);
	}
	else
	  Logging::printf("not finished %x,%x inprogress %x\n", fis[0], fis[4], _inprogress);
	break;
      case 0x41: // dma setup fis
	assert(fislen == 7);
	copy_offset = 0;
	break;
      case 0x5f: // pio setup fis
	assert(fislen == 5);
	copy_offset = 0x20;

	Logging::printf("PIO setup fis\n");
	break;
      default:
	assert(!"Invalid D2H FIS!");
      }

    // copy to user
    if (cmd & 0x10)  copy_out(fisbase + copy_offset, fis, fislen * 4);
    if (fis[0] & 0x4000) _parent->trigger_irq(this);
  };


  bool set_drive(FisReceiver *drive)
  {
    if (_drive) return true;
    _drive = drive;
    _drive->set_peer(this);

    comreset();
    return false;
  }


  void comreset()
  {
    // reset all device registers except CL+FB to default values
    const char *names[] = {"PxIS", "PxIE", "PxCMD", "PxTFD", "PxSIG", "PxSSTS", "PxSCTL", "PxSERR", "PxCI", "PxSNTF", "PxFBS"};
    for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]); i++)
      HwRegisterSet<AhciPort>::reset_reg(HwRegisterSet<AhciPort>::find_reg(names[i]));

    _need_initial_fis = true;
    _inprogress = 0;

    if (_drive) {

      // we use the legacy reset mechanism to transmit a COMRESET to the SATA disk
      unsigned fis[5] = { 0x27, 0, 0, 0x04000000, 0};
      _drive->receive_fis(5, fis);
      // toggle SRST in the control register
      fis[3] = 0;
      _drive->receive_fis(5, fis);
    }
  }


  unsigned execute_command(unsigned value)
  {
    COUNTER_INC("ahci cmd");
      unsigned clbase = 0;
      check2(!HwRegisterSet<AhciPort>::read_reg(HwRegisterSet<AhciPort>::find_reg("PxCLB"), clbase));

      // try to execute all active commands
      for (unsigned i = 0; i < 32; i++)
	{
	  unsigned slot = (_ccs >= 31) ? 0 : _ccs + 1;
	  _ccs = slot;

	  // XXX check for busy bit
	  if (value & ~_inprogress & (1 << slot))
	    {
	      unsigned status = 0;
	      if (!HwRegisterSet<AhciPort>::read_reg(_reg_tfd, status) || status & 0x80)  break;
	      _inprogress |= 1 << slot;

	      unsigned  cl[4];
	      copy_in(clbase +  slot * 0x20, cl, sizeof(cl));
	      unsigned clflags  = cl[0];

	      unsigned ct[clflags & 0x1f];
	      copy_in(cl[2], ct, sizeof(ct));
	      assert(~clflags & 0x20 && "ATAPI unimplemented");

	      // send a dma_setup_fis
	      // we reuse the reserved fields to send the PRD count and the slot
	      unsigned dsf[7] = {0x41, cl[2] + 0x80, 0, clflags >> 16, 0, cl[1], slot+1};
	      _drive->receive_fis(7, dsf);

	      // set BSY
	      HwRegisterSet<AhciPort>::modify_reg(_reg_tfd, 0x00, 0x80);
	      _drive->receive_fis(clflags & 0x1f, ct);
	    }
	}
      // make _css known
      modify_reg(_reg_cmd, 0x1f00, (_ccs & 0x1f) << 8);
      return 0;
  }


  void write_callback(unsigned flags, unsigned oldvalue, unsigned newvalue)
  {
    switch (flags) {
    case CALLBACK_CI:
      execute_command(newvalue);
      break;
    case CALLBACK_CMD:
      // enable FRE
      if ( newvalue & 0x10 && ~oldvalue & 0x10) newvalue |= 1 << 14;
      // disable FRE
      if (~newvalue & 0x10 &&  oldvalue & 0x10) newvalue &= ~(1 << 14);

      // enable CR
      if (newvalue & 1 && ~oldvalue & 1) { newvalue |= 1 << 15;  _ccs = 32; }

      if (~newvalue & 1 &&  oldvalue & 1)
	{
	  // disable CR
	  newvalue &= ~(1 << 15);
	  // reset PxSACT
	  HwRegisterSet<AhciPort>::reset_reg(_reg_sact);
	  HwRegisterSet<AhciPort>::reset_reg(_reg_ci);
	}
      modify_reg(_reg_cmd, ~0U, newvalue);
      break;
    case CALLBACK_SCTL:
      switch (newvalue & 0xf) {
      case 1: comreset(); break;
      case 2: // put device in offline mode
	HwRegisterSet<AhciPort>::reset_reg(HwRegisterSet<AhciPort>::find_reg("PxSSTS"));
      default:
	break;
      }
      break;
    case CALLBACK_IS:
      COUNTER_INC("IS");
      break;
    default:
      Logging::panic("%s flags %x\n", __PRETTY_FUNCTION__, flags);
    }
  }

  AhciPort() : _drive(0), _parent(0)
    {
      _reg_tfd = HwRegisterSet<AhciPort>::find_reg("PxTFD");
      _reg_cmd = HwRegisterSet<AhciPort>::find_reg("PxCMD");
      _reg_ci  = HwRegisterSet<AhciPort>::find_reg("PxCI");
      _reg_sact  = HwRegisterSet<AhciPort>::find_reg("PxSACT");
    };

};


/**
 * An AhciController on a PCI card.
 *
 * State: unstable
 * Features: PCI cfg space, AHCI register set
 */
class AhciController : public PciDeviceConfigSpace<AhciController>,
		       public HwRegisterSet<AhciController>,
		       public ParentIrqProvider,
		       public StaticReceiver<AhciController>
{
 public:
  enum {
    CALLBACK_GHC = 1,
    MAX_PORTS = 32,
  };

 private:

  DBus<MessageIrq> &_bus_irqlines;
  unsigned char _irq;

  int _pci_bar_reg;
  int _pci_cmd_reg;
  int _reg_ghc;
  int _reg_is;
  AhciPort _ports[MAX_PORTS];

  const char* debug_getname() { return "AHCI"; }


  int init(Motherboard &mb)
  {
#define ICHECK(X)  { int __res = X; if (__res < 0) return __res; }

    // search for PCI config registers
    ICHECK(_pci_bar_reg = PciDeviceConfigSpace<AhciController>::find_reg("ABAR"));
    ICHECK(_pci_cmd_reg = PciDeviceConfigSpace<AhciController>::find_reg("CMD"));
    // and device register
    ICHECK(_reg_ghc = HwRegisterSet<AhciController>::find_reg("GHC"));
    ICHECK(_reg_is = HwRegisterSet<AhciController>::find_reg("IS"));
    for (unsigned i=0; i < MAX_PORTS; i++) _ports[i].set_parent(this, &mb.bus_memwrite, &mb.bus_memread);
    return 0;
  }

 public:

  void trigger_irq (void * child) {
    unsigned value = 0;
    HwRegisterSet<AhciController>::read_reg(_reg_is, value);
    unsigned index = reinterpret_cast<AhciPort *>(child) - _ports;
    if (~value & (1 << index))
      {
	HwRegisterSet<AhciController>::modify_reg(_reg_is, 0, 1 << index);
	if (HwRegisterSet<AhciController>::read_reg(_reg_ghc, value) && value & 0x2)
	  {
	    MessageIrq msg(MessageIrq::ASSERT_IRQ, _irq);
	    _bus_irqlines.send(msg);
	  }
      }
  };


  void write_callback(unsigned flags, unsigned oldvalue, unsigned newvalue)
  {
    switch (flags) {
    case CALLBACK_GHC:
      // reset HBA?
      if (newvalue & 1)
	{
	  for (unsigned i=0; i < MAX_PORTS; i++)  _ports[i].comreset();
	  // set all registers to default values
	  HwRegisterSet<AhciController>::reset_reg(_reg_is);
	  HwRegisterSet<AhciController>::reset_reg(_reg_ghc);
	}
      break;
    default:
      Logging::panic("%s flags %x\n", __PRETTY_FUNCTION__, flags);
    }
  }


  bool receive(MessageMemWrite &msg)
  {
    unsigned long addr = msg.phys;
    unsigned cmd_value;
    if (!match_bar(_pci_bar_reg, addr) || !(PciDeviceConfigSpace<AhciController>::read_reg(_pci_cmd_reg, cmd_value) && cmd_value & 0x2))
      return false;

    if (msg.count != 4 || (addr & 0x3)) {
      Logging::printf("%s() - unaligned or non-32bit access at %lx, %x\n", __PRETTY_FUNCTION__, msg.phys, msg.count);
      return false;
    };

    bool res = false;
    if (addr < 0x100)
      res = HwRegisterSet<AhciController>::write_all_regs(addr, *reinterpret_cast<unsigned *>(msg.ptr), 4, this);
    else if (addr < 0x100+MAX_PORTS*0x80)
	res = _ports[(addr - 0x100) / 0x80].write_all_regs(addr & 0x7f, *reinterpret_cast<unsigned *>(msg.ptr), 4, &_ports[(addr - 0x100) / 0x80]);
    else
      return false;


    if (!res)  Logging::panic("%s(%lx)\n", __func__, addr);
    return true;
  };


  bool receive(MessageMemRead &msg)
  {
    unsigned long addr = msg.phys;
    unsigned cmd_value;
    if (!match_bar(_pci_bar_reg, addr) || !(PciDeviceConfigSpace<AhciController>::read_reg(_pci_cmd_reg, cmd_value) && cmd_value & 0x2))
      return false;

    if (msg.count != 4 || (addr & 0x3)) {
      Logging::printf("%s() - unaligned or non-32bit access at %lx, %x\n", __PRETTY_FUNCTION__, msg.phys, msg.count);
      return false;
    };

    bool res;
    unsigned uvalue = 0;
    if (addr < 0x100)
      res = HwRegisterSet<AhciController>::read_all_regs(addr, uvalue, 4);
    else if (addr < 0x100+MAX_PORTS*0x80)
      res = _ports[(addr - 0x100) / 0x80].read_all_regs(addr & 0x7f, uvalue, 4);
    else
      return false;

    if (res)  *reinterpret_cast<unsigned *>(msg.ptr) = uvalue;
    return res;
  };


  bool receive(MessageAhciSetDrive &msg)
  {
    if (msg.port > MAX_PORTS || _ports[msg.port].set_drive(msg.drive)) return false;

    // enable it in the PI register
    HwRegisterSet<AhciController>::modify_reg(HwRegisterSet<AhciController>::find_reg("PI"), 0, 1 << msg.port);

    /**
     * fix CAP, according to the spec this is unnneeded, but Linux
     * 2.6.24 checks and sometimes crash without it!
     */
    unsigned count = 0;
    unsigned value;
    if (HwRegisterSet<AhciController>::read_reg(HwRegisterSet<AhciController>::find_reg("PI"), value))
      {
	for (;value; value >>= 1) { count += value & 1; }
	HwRegisterSet<AhciController>::modify_reg(HwRegisterSet<AhciController>::find_reg("CAP"), 0x1f, (count - 1));
      }
    return true;
  }


  bool receive(MessagePciConfig &msg)  {  return PciDeviceConfigSpace<AhciController>::receive(msg); };


  AhciController(Motherboard &mb, unsigned char irq) : _bus_irqlines(mb.bus_irqlines), _irq(irq)
  {
    init(mb);
  };
};


REGISTERSET(AhciPort,
	    REGISTER_RW("PxCLB",    0x0*8, 32, 0, 0xfffffc00),
	    REGISTER_RO("PxCLBU",   0x4*8, 32, 0),
	    REGISTER_RW("PxFB",     0x8*8, 32, 0, 0xffffff00),
	    REGISTER_RO("PxFBU",    0xc*8, 32, 0),
	    REGISTER   ("PxIS",    0x10*8, 32, 0, 0xdfc000af, 0, 0xdfc000af, AhciPort::CALLBACK_IS),
	    REGISTER_RW("PxIE",    0x14*8, 32, 0, 0x7dc0007f),
	    REGISTER   ("PxCMD",   0x18*8, 32, 0, 0xf3000011, 0, 0, AhciPort::CALLBACK_CMD),
	    REGISTER_PR("PxTFD",   0x20*8, 32, 0x7f),
	    REGISTER_PR("PxSIG",   0x24*8, 32, 0xffffffff),
	    REGISTER_PR("PxSSTS",  0x28*8, 32, 0),
	    REGISTER   ("PxSCTL",  0x2c*8, 32, 0, 0x00000fff, 0, 0, AhciPort::CALLBACK_SCTL),
	    REGISTER   ("PxSERR",  0x30*8, 32, 0, 0xffffffff, 0, 0xffffffff, 0),
	    REGISTER   ("PxSACT",  0x34*8, 32, 0, 0xffffffff, 0xffffffff, 0, 0),
	    REGISTER   ("PxCI",    0x38*8, 32, 0, 0xffffffff, 0xffffffff, 0, AhciPort::CALLBACK_CI),
	    REGISTER_RO("PxSNTF",  0x3c*8, 32, 0),
	    REGISTER_RO("PxFBS",   0x40*8, 32, 0));


REGISTERSET(PciDeviceConfigSpace<AhciController>,
	    REGISTER_RO("ID",   0x0*8, 32, 0x275c8086),
	    REGISTER_RW("CMD",  0x4*8, 16, 0, 0x0406),
	    REGISTER_RO("STS",  0x6*8, 16, 0x10),
	    REGISTER_RO("RID",  0x8*8,  8, 0x02),
	    REGISTER_RO("CC",   0x9*8, 24, 0x010601),
	    REGISTER_RW("ABAR",0x24*8, 32, 0, 0xffffe000),
	    REGISTER_RO("SS",  0x2c*8, 32, 0x275c8086),
	    REGISTER_RO("CAP", 0x34*8,  8, 0x80),
	    REGISTER_RW("INTR",0x3c*8, 16, 0x0100, 0xff),
	    REGISTER_RO("PID", 0x80*8, 16, 0x0001),
	    REGISTER_RO("PC",  0x82*8, 16, 0x0000),
	    REGISTER_RO("PMCS",0x84*8, 16, 0x0000));



REGISTERSET(AhciController,
	    REGISTER_PR("CAP",   0x0*8, 32, 0x40149f00 | (AhciController::MAX_PORTS - 1)),
	    REGISTER   ("GHC",   0x4*8, 32, 0x80000000, 0x3, 0x1, 0, AhciController::CALLBACK_GHC),
	    REGISTER   ("IS",    0x8*8, 32, 0, 0xffffffff, 0x00000000, 0xffffffff, 0),
	    REGISTER_PR("PI",    0xc*8, 32, 1),
	    REGISTER_RO("VS",   0x10*8, 32, 0x00010200));


PARAM(ahci,
      {
	AhciController *dev = new AhciController(mb, argv[1]);
	mb.bus_memwrite.add(dev, &AhciController::receive_static<MessageMemWrite>);
	mb.bus_memread.add(dev, &AhciController::receive_static<MessageMemRead>);

	// register PCI device
	unsigned dstbdf = argv[2];
	MessagePciBridgeAdd msg(dstbdf & 0xff, dev, &AhciController::receive_static<MessagePciConfig>);
	if (!mb.bus_pcibridge.send(msg, dstbdf >> 8))
	  Logging::printf("could not add PCI device to %x\n", dstbdf);

	// register for AhciSetDrive messages
	mb.bus_ahcicontroller.add(dev, &AhciController::receive_static<MessageAhciSetDrive>);

	// set default state, this is normally done by the BIOS
	// enable IRQ, busmaster DMA and memory accesses
	dev->PciDeviceConfigSpace<AhciController>::write_reg(dev->PciDeviceConfigSpace<AhciController>::find_reg("CMD"), 0x406, true);
	// set MMIO region and IRQ
	dev->PciDeviceConfigSpace<AhciController>::write_reg(dev->PciDeviceConfigSpace<AhciController>::find_reg("ABAR"), argv[0], true);
	dev->PciDeviceConfigSpace<AhciController>::write_reg(dev->PciDeviceConfigSpace<AhciController>::find_reg("INTR"), argv[1], true);
      },
      "ahci:mem,irq,bdf - attach an AHCI controller to a PCI bus.",
      "Example: Use 'ahci:0xe0800000,14,0x30' to attach an AHCI controller to 00:06.0 on address 0xe0800000 with irq 14.",
      "The AHCI controllers are automatically numbered, starting with 0."
      );
