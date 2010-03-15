/**
 * Virtual Bios executor.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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

#include "nul/motherboard.h"
#include "executor/bios.h"


/**
 * Provide a realmode BIOS.
 *
 * State: unstable
 * Features: IDT, ResetVector, Keyboard, Timer, Disk
 * Missing: EBDA, ACPI, PCIBios, PCIRoutingInfo, MultipleDisks...
 *
 * Please note that VGA BIOS routines are provided in vga.
 */
class VirtualBios : public StaticReceiver<VirtualBios>, public BiosCommon
{
  unsigned long _base;
  unsigned long _memsize;
  unsigned char _resetvector[16];

  const char* debug_getname() { return "VirtualBios"; }
  void debug_dump() {
    Device::debug_dump();
    Logging::printf(" base 0x%lx", _base);
  }


  /**
   * Out to IO-port.
   */
  void outb(unsigned short port, unsigned value)
  {
    MessageIOOut msg(MessageIOOut::TYPE_OUTB, port, value);
    _mb.bus_ioout.send(msg);
  }


  /**
   * Do an realmode iret to the user.
   */
  bool do_iret(CpuState *cpu)
  {
    /**
     * We jump to the last instruction in the 16-byte reset area where
     * we provide an IRET instruction to the instruction emulator.
     */
    cpu->cs.sel  = 0xffff;
    cpu->cs.base = 0xffff0000;
    cpu->eip = 0xffff;

    // we have to fix the eflags on the user stack!
    unsigned char oldflags;
    copy_in(cpu->ss.base + cpu->esp + 4, &oldflags, 1);
    copy_out(cpu->ss.base + cpu->esp + 4, &cpu->efl, 1);

    return true;
  }


  /**
   * called on reset.
   */
  void reset_helper()
  {
    // initialize realmode idt
    unsigned value = (_base >> 4) << 16;
    for (unsigned i=0; i<256; i++) {
      copy_out(i*4, &value, 4);
      value++;
    }


    // initialize PIT0
    // let counter0 count with minimal freq of 18.2hz
    outb(0x40+3, 0x24);
    outb(0x40+0, 0);

    // let counter1 generate 15usec refresh cycles
    outb(0x40+3, 0x56);
    outb(0x40+1, 0x12);


    // the master PIC
    // ICW1-4+IMR
    outb(0x20+0, 0x11);
    outb(0x20+1, 0x08); // offset 8
    outb(0x20+1, 0x04); // has slave on 2
    outb(0x20+1, 0x0f); // is buffer, master, AEOI and x86
    outb(0x20+1, 0xfc);

    // the slave PIC + IMR
    outb(0xa0+0, 0x11);
    outb(0xa0+1, 0x70); // offset 0x70
    outb(0xa0+1, 0x02); // is slave on 2
    outb(0xa0+1, 0x0b); // is buffer, slave, AEOI and x86
    outb(0xa0+1, 0xff);


    // initilize bios data area
    // we have 640k low memory
    write_bda(0x13, 640, 2);
    // keyboard buffer
    write_bda(0x1a, 0x1e001e, 4);
    write_bda(0x80, 0x2f001e, 4);


#if 0
    // XXX announce number of serial ports
    // announce port
    write_bios_data_area(mb, serial_count * 2 - 2,      argv[0]);
    write_bios_data_area(mb, serial_count * 2 - 2 + 1,  argv[0] >> 8);

    // announce number of serial ports
    write_bios_data_area(mb, 0x11, read_bios_data_area(mb, 0x11) & ~0xf1 | (serial_count*2));
#endif
  };


  /**
   * Memory+PS2 INT.
   */
  bool handle_int15(CpuState *cpu)
  {
    COUNTER_INC("int15");
    // default is to clear CF
    cpu->efl &= ~1;
    switch (cpu->ax)
      {
      case 0x2400: // disable A20
      case 0x2401: // enable  A20
	{
	  MessageLegacy msg(MessageLegacy::GATE_A20, cpu->al);
	  if (_mb.bus_legacy.send(msg))
	    cpu->ax = 0;
	  else
	    error(cpu, 0x24);
	}
	break;
      case 0xc201:            // reset mouse
	{
	  cpu->ax = 0x0001;
	  cpu->bx = 0xfaaa;
	}
	break;
      case 0xe820:           // get memory map
	{
	  if ((cpu->edx == 0x534D4150 && cpu->ecx >= 20))
	    {
	      Logging::printf("%s() ebx %x sizeof(mymap) %x rdi %x memsize %lx\n", __func__, cpu->ebx, 2, cpu->edi, _memsize);
	      struct mmap{
		unsigned long long base;
		unsigned long long size;
		unsigned type;
	      } mmap;
	      mmap.type = 1;
	      switch (cpu->ebx)
		{
		case 0:
		  mmap.base = 0;
		  mmap.size = 0xa0000;
		  cpu->ebx++;
		  break;
		case 1:
		  mmap.base = 1<<20;
		  mmap.size = _memsize - (1<<20);
		  cpu->ebx = 0;
		  break;
		default:
		  mmap.type = 0;
		  break;
		}

	      if (mmap.type)
		{
		  copy_out(cpu->es.base + cpu->di, &mmap, 20);
		  cpu->eax = cpu->edx;
		  cpu->ecx = 20;
		  cpu->edx = 0;
		  break;
		}
	    }
	  goto unsupported;
	}
      case 0x00c0:            // get rom configtable
      case 0x5300 ... 0x53ff: // apm installation check
      case 0x8800 ... 0x88ff: // get extended memory
      case 0xc000 ... 0xc0ff: // get configuration
      case 0xc100 ... 0xc1ff: // get ebda
      case 0xe801:            // get memsize
      case 0xe980:            // get intel speedstep information
      unsupported:
	// unsupported
	DEBUG;
	error(cpu, 0x86);
	break;
      default:
	VB_UNIMPLEMENTED;
      }
    return do_iret(cpu);
  }


 public:


  /**
   * The memory read routine for the last 16byte below 4G and below 1M.
   */
  bool receive(MessageMem &msg)
  {
    if (!msg.read || !in_range(msg.phys, 0xfffffff0, 0x10) && !in_range(msg.phys, 0xffff0, 0x10))  return false;
    *msg.ptr = *reinterpret_cast<unsigned *>(_resetvector + (msg.phys & 0xc));
    return true;
  }


  /**
   * Execute BIOS code.
   */
  bool  receive(MessageExecutor &msg)
  {
    CpuState *cpu = msg.cpu;
    assert(cpu->head._pid == 33);

    // works only in real and x86mode
    if (cpu->pm() && !cpu->v86()) return false;
    if (in_range(cpu->cs.base + cpu->eip, _base, MAX_VECTOR))
      {
	COUNTER_INC("VB");
	unsigned irq =  (cpu->cs.base + cpu->eip) - _base;
	MessageBios msg2(msg, irq);
	if (irq != RESET_VECTOR && _mb.bus_bios.send(msg2, true))  return do_iret(cpu);
	switch (irq)
	  {
	  case 0x0f:  return do_iret(cpu);
	  case 0x10:  return do_iret(cpu);
	  case 0x11: // BIOS equipment word
	    cpu->ax = 0x34; // 80x25, ps2-mouse, no-floppy
	    return do_iret(cpu);
	  case 0x12: // get low memory
	    cpu->ax = read_bda(0x13);
	    return do_iret(cpu);
	  case 0x15:  return handle_int15(cpu);
	  case 0x17:  // printer
	    cpu->efl |= 1;  // unsupported
	    return do_iret(cpu);
	  case RESET_VECTOR:
	    reset_helper();
	    // notify optionroms lately
	    _mb.bus_bios.send(msg2);
	    return jmp_int(cpu, 0x19);
	  default:
	    VB_UNIMPLEMENTED;
	  }
      }
    return false;
  }


  VirtualBios(unsigned short segment, Motherboard &mb)  : BiosCommon(mb), _base(static_cast<unsigned>(segment) << 4)
    {
      MessageHostOp msg1(MessageHostOp::OP_GUEST_MEM, 0);
      if (!mb.bus_hostop.send(msg1))
	Logging::panic("%s can't get physical memory size", __PRETTY_FUNCTION__);
      _memsize = msg1.len;

      // initialize the reset vector with noops
      memset(_resetvector, 0x90, sizeof(_resetvector));
      // realmode longjump to reset vector
      _resetvector[0x0] = 0xea;
      _resetvector[0x1] = RESET_VECTOR & 0xff;
      _resetvector[0x2] = RESET_VECTOR >> 8;
      _resetvector[0x3] = _base >> 4;
      _resetvector[0x4] = _base >> 12;

      // the iret for do_iret()
      _resetvector[0xf] = 0xcf;

      mb.bus_mem.add(this,         &VirtualBios::receive_static<MessageMem>);
    }
};

PARAM(vbios,
      {
	mb.bus_executor.add(new VirtualBios(argv[0], mb),  &VirtualBios::receive_static, 33);
      },
      "vbios:segment - create a executor that emulates a virtual bios.",
      "Example: 'vbios:0xf000'.");
