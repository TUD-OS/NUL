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
#include "models/dma.h"
#include "models/keyboard.h"
#include "vmm/motherboard.h"
#include "executor/bios.h"


/**
 * Provide a realmode BIOS.
 *
 * State: unstable
 * Features: IDT, ResetVector, Keyboard, Timer, Disk
 * Missing: EBDA, ACPI, PCIBios, PCIRoutingInfo, MultipleDisks...
 *
 * Please note that Vesa and VGA BIOS routines are provided in vesabios and vga.
 */
class VirtualBios : public StaticReceiver<VirtualBios>, public BiosCommon
{
  enum
  {
    MAGIC_DISK_TAG = ~0u,
    FREQ = 1000,
    DISK_TIMEOUT = 5000,
    DISK_COMPLETION_CODE = 0x79,
  };

  unsigned long _base;
  unsigned long _memsize;
  unsigned char _resetvector[16];
  Motherboard *_hostmb;
  unsigned _lastkey;
  unsigned _timer;
  DiskParameter _disk_params;

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
   * Jump to another realmode INT handler.
   */
  bool jmp_int(CpuState *cpu, unsigned char number)
  {
    unsigned short offset, segment;
    copy_in(number*4+0, &offset, 2);
    copy_in(number*4+2, &segment, 2);

    // As this is only called within our own IDT handler cs.limit and
    // cs.ar are fine
    cpu->cs.sel = segment;
    cpu->cs.base = segment << 4;
    cpu->eip = offset;
    if (segment != 0xf000) Logging::printf("%s() %x - %x\n", __func__, segment, offset);

    // we are done with the emulation
    return true;
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
    unsigned segment = _base >> 4;
    for (unsigned i=0; i<256; i++)
      {
	copy_out(i*4, &i, 2);
	copy_out(i*4+2, &segment, 2);
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

  bool boot_from_disk(CpuState *cpu, VirtualCpuState *vcpu)
  {
    cpu->cs.sel = 0;
    cpu->cs.base = 0;
    cpu->eip = 0x7c00;
    cpu->efl = 2;

    // we push an iret frame on the stack
    cpu->ss.sel  = 0;
    cpu->ss.base = 0;
    cpu->esp     = 0x7000;
    copy_out(cpu->ss.base + cpu->esp + 0, &cpu->eip,    2);
    copy_out(cpu->ss.base + cpu->esp + 2, &cpu->cs.sel, 2);
    copy_out(cpu->ss.base + cpu->esp + 4, &cpu->efl,    2);

    cpu->edx = 0x80; // booting from first disk
    if (!disk_op(cpu, vcpu, 0, 0x7c00, 1, false) || cpu->ah)
      Logging::panic("VB: could not read MBR from boot disk");
    return true;
  }


  /**
   * Converts our internal keycode format into the BIOS one.
   */
  static unsigned keycode2bios(unsigned value)
  {
    static struct {
      unsigned short code;
      unsigned keycode;
    } bios_key_map[] = {
      { 72 << 8,  KBFLAG_EXTEND0 | 0x75}, // up
      { 80 << 8,  KBFLAG_EXTEND0 | 0x72}, // down
      { 77 << 8,  KBFLAG_EXTEND0 | 0x74}, // right
      { 75 << 8,  KBFLAG_EXTEND0 | 0x6b}, // left
      { 59 << 8,  0x05}, // F1
      { 60 << 8,  0x06}, // F2
      { 61 << 8,  0x04}, // F3
      { 62 << 8,  0x0c}, // F4
      { 63 << 8,  0x03}, // F5
      { 64 << 8,  0x0b}, // F6
      { 65 << 8,  0x83}, // F7
      { 66 << 8,  0x0a}, // F8
      { 67 << 8,  0x01}, // F9
      { 68 << 8,  0x09}, // F10
      {133 << 8, 0x78}, // F11
      {134 << 8, 0x07}, // F12
      { 71 << 8,  KBFLAG_EXTEND0 | 0x6c}, // home
      { 82 << 8,  KBFLAG_EXTEND0 | 0x70}, // insert
      { 83 << 8,  KBFLAG_EXTEND0 | 0x71}, // delete
      { 79 << 8,  KBFLAG_EXTEND0 | 0x69}, // end
      { 73 << 8,  KBFLAG_EXTEND0 | 0x7d}, // pgup
      { 81 << 8,  KBFLAG_EXTEND0 | 0x7a}, // pgdown
      {110 << 8,   0x76},                 // esc
      {       8,   0x66},                 // backspace
    };

    value = value & ~KBFLAG_NUM;

    // handle both shifts the same
    if (value & KBFLAG_RSHIFT) value = value & ~KBFLAG_RSHIFT | KBFLAG_LSHIFT;
    for (unsigned i=0; i < sizeof(bios_key_map) / sizeof(bios_key_map[0]); i++)
      if (bios_key_map[i].keycode == value)
	return bios_key_map[i].code;
    unsigned *ascii_map = GenericKeyboard::get_ascii_map();
    for (unsigned i=0; i<128; i++)
      if (ascii_map[i] == value)
	return (value << 8) | i;
    return 0;
  }


  void error(CpuState *cpu, unsigned char errorcode)
  {
    cpu->efl |= 1;
    cpu->ah = errorcode;

  }


  /**
   * Handle the HW timer IRQ.
   */
  bool handle_int08(CpuState *cpu)
  {
    // Note: no need for EOI since we run in AEOI mode!
    //DEBUG;
    // increment BIOS tick counter
    unsigned ticks = read_bda(0x6c);
    ticks++;
    // midnight?
    if (ticks >= 0x001800B0)
      {
	ticks = 0;
	write_bda(0x70, read_bda(0x70)+1, 1);
      }
    write_bda(0x6c, ticks, 4);

    return jmp_int(cpu, 0x1c);
  }


  /**
   * Handle the Keyboard IRQ.
   */
  bool handle_int09(CpuState *cpu)
  {
    //DEBUG;
    MessageIrq msg(MessageIrq::ASSERT_IRQ, 1);
    _hostmb->bus_hostirq.send(msg);
    return do_iret(cpu);
  }


  bool check_drive(CpuState *cpu)
  {
    if (cpu->dl == 0x80) return true;
    error(cpu, 0x01); // invalid parameter
    return false;
  }

  /**
   * Read/Write disk helper.
   */
  bool disk_op(CpuState *cpu, VirtualCpuState *vcpu, unsigned long long blocknr, unsigned long address, unsigned count, bool write)
  {
    DmaDescriptor dma;
    dma.bytecount  = 512*count;
    dma.byteoffset = address;

    Logging::printf("%s(%llx) %s count %x -> %lx\n", __func__, write ? "write" : "read", blocknr, count, address);
    MessageDisk msg2(MessageDisk::DISK_READ, cpu->dl & 0x7f, MAGIC_DISK_TAG, blocknr, 1, &dma, 0, ~0ul);
    if (!_mb.bus_disk.send(msg2) || msg2.error)
      {
	Logging::printf("msg2.error %x\n", msg2.error);
	error(cpu, 0x01);
	return do_iret(cpu);
      }
    else
      {
	// wait for completion needed for AHCI backend!
	Cpu::atomic_or<volatile unsigned>(&vcpu->hazard, VirtualCpuState::HAZARD_BIOS);

	// prog timeout during wait
	MessageTimer msg3(_timer, _mb.clock()->abstime(DISK_TIMEOUT, FREQ));
	_mb.bus_timer.send(msg3);

	cpu->cs.base = _base;
	cpu->eip = WAIT_DISK_VECTOR;
	return true;
      }
  }


  /**
   * Disk INT.
   */
  bool handle_int13(CpuState *cpu, VirtualCpuState *vcpu)
  {
    COUNTER_INC("int13");
    struct disk_addr_packet {
      unsigned char size;
      unsigned char res;
      unsigned short count;
      unsigned short offset;
      unsigned short segment;
      unsigned long long block;
    } da;

    // default clears CF
    cpu->efl &= ~1;

    switch (cpu->ah)
      {
      case 0x00: // reset disk
	goto reset_disk;
      case 0x02: // read
      case 0x03: // write
	if (check_drive(cpu))
	  {
	    unsigned cylinders = cpu->ch | (cpu->cl << 2) & 0x300;
	    unsigned heads =  cpu->dh;
	    unsigned sectors = cpu->cl & 0x3f;
	    unsigned blocknr;
	    if (cpu->dl & 0x80)
	      blocknr = (cylinders * 255 + heads) * 63 + sectors - 1;
	    else
	      blocknr = (cylinders * 2 + heads) * 18 + sectors - 1;
	    return disk_op(cpu, vcpu, blocknr, cpu->es.base + cpu->bx, cpu->al, cpu->ah & 1);
	  }
	break;
      case 0x08: // get drive params
	// we report a single drive with maximum parameters
	if (check_drive(cpu))
	  {
	    cpu->cx = 0xfeff;
	    cpu->dx = 0xfe01;
	    cpu->ah = 0;  // successful
	  }
	break;
      case 0x15: // get disk type
	if (check_drive(cpu))
	  {
	    cpu->ah = 0x03;  // we report a harddisk
	    unsigned sectors = (_disk_params.sectors >> 32) ? 0xffffffff : _disk_params.sectors;
	    cpu->dx = sectors & 0xffff;
	    cpu->cx = sectors >> 16;
	  }
	break;
      case 0x41:  // int13 extension supported?
	if (check_drive(cpu))
	  switch (cpu->bx)
	    {
	    case 0x55aa:
	      // we report that version1 is supported
	      cpu->ah = 0x01;
	      cpu->cx = 0x0001;
	      cpu->bx = 0xaa55;
	      break;
	    default:
	      VB_UNIMPLEMENTED;
	    }
	break;
      reset_disk:
      case 0x0d: // reset disk
	if (check_drive(cpu))  cpu->ah = 0x00; // successful
	break;
      case 0x42: // extended read
      case 0x43: // extended write
	if (check_drive(cpu))
	  {
	    copy_in(cpu->ds.base + cpu->si, &da, sizeof(da));
	    return disk_op(cpu, vcpu, da.block, (da.segment << 4) + da.offset, da.count, cpu->ah & 1);
	  }
	break;
      case 0x48: // get drive params extended
	if (check_drive(cpu))
	  {
	    struct drive_parameters
	    {
	      unsigned short size;
	      unsigned short flags;
	      unsigned pcylinders;
	      unsigned pheads;
	      unsigned psectors;
	      unsigned long long sectors;
	      unsigned short sectorsize;
	    } params;
	    params.flags = 2;
	    params.sectors = _disk_params.sectors;
	    params.pheads = 255;
	    params.psectors = 63;
	    unsigned long long sectors =  _disk_params.sectors;
	    Math::div64(sectors, params.psectors*params.pheads);
	    params.pcylinders = sectors;
	    params.size = 0x1a;
	    params.sectorsize = 512;
	    copy_out(cpu->ds.base + cpu->si, &params, params.size);
	    Logging::printf("VB: driveparam size %x sectors %llx\n", params.size, params.sectors);
	    cpu->ah = 0; // function supported
	  }
	break;
      default:
	switch (cpu->ax)
	  {
	  case 0x4b00:  // bootable CDROM Emulation termintate
	  case 0x4b01:  // bootable CDROM Emulation status
	    error(cpu, 0x4b);
	    break;
	  default:
	    VB_UNIMPLEMENTED;
	  }
      }
    return do_iret(cpu);
  }


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


  /**
   * Keyboard INT handler.
   */
  bool handle_int16(CpuState *cpu)
  {
    //COUNTER_INC("int16");
    //DEBUG;
    unsigned short next  = read_bda(0x1a);
    unsigned short first = read_bda(0x1c);
    unsigned short start = read_bda(0x80);
    unsigned short end   = read_bda(0x82);

    switch (cpu->ah)
      {
      case 0x00:  // get keystroke
	{
	  cpu->ax = read_bda(next);
	  if (first != next)
	    {
	      next += 2;
	      if (next > end)
		next = start;
	      write_bda(0x1a, next, 2);
	    }
	}
	break;
      case 0x01: // check keystroke
	{
	  if (first != next)
	    {
	      cpu->efl &= ~0x40;
	      cpu->ax = read_bda(next);
	    }
	  else
	    cpu->efl |= 0x40;
	break;
	}
      case 0x02: // get shift flag
	cpu->al = 0;
	if (_lastkey & KBFLAG_NUM)                    cpu->al |= 1 << 5;
	if (_lastkey & (KBFLAG_LALT | KBFLAG_RALT))   cpu->al |= 1 << 3;
	if (_lastkey & (KBFLAG_LCTRL | KBFLAG_RCTRL)) cpu->al |= 1 << 2;
	if (_lastkey & KBFLAG_LSHIFT)                 cpu->al |= 1 << 1;
	if (_lastkey & KBFLAG_RSHIFT)                 cpu->al |= 1 << 0;
	break;
      case 0x03: // set typematic
	// ignored
	break;
      default:
	VB_UNIMPLEMENTED;
      }
    //pDEBUG;
    return do_iret(cpu);
  }

  unsigned char tobcd(unsigned char v) { return ((v / 10) << 4) | (v % 10); }

  /**
   * Time INT handler.
   */
  bool handle_int1a(CpuState *cpu)
  {
    COUNTER_INC("int1a");
    switch (cpu->ah)
      {
      case 0x00: // get system time
	{
	  unsigned ticks = read_bda(0x6c);
	  cpu->al = read_bda(0x70);
	  cpu->dx = ticks;
	  cpu->cx = ticks >> 16;
	  break;
	}
      case 0x01: // set system time
	write_bda(0x6c, static_cast<unsigned>(cpu->cx)<< 16 | cpu->dx, 4);
	write_bda(0x70, 0, 1);
	break;
      case 0x02: // realtime clock
	{
	  unsigned seconds = _mb.clock()->clock(1);
	  cpu->ch = tobcd((seconds / 3600) % 24);
	  cpu->cl = tobcd((seconds / 60) % 60);
	  cpu->dh = tobcd(seconds % 60);
	  cpu->dl = 0;
	  cpu->efl &= ~1;
	  Logging::printf("realtime clock %x:%x:%x %d\n", cpu->ch, cpu->cl, cpu->dh, seconds);
	  break;
	}
      default:
	VB_UNIMPLEMENTED;
      }
    return do_iret(cpu);
  }


 public:

  /**
   * Get disk commit.
   */
  bool  receive(MessageDiskCommit &msg)
  {
    if (msg.usertag == MAGIC_DISK_TAG)
      {
	Cpu::atomic_and<volatile unsigned>(&_mb.vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_BIOS);
	write_bda(DISK_COMPLETION_CODE, msg.status, 1);
	return true;
      }
    return false;
  }

  /**
   * Get disk timeout.
   */
  bool  receive(MessageTimeout &msg)
  {
    if (msg.nr == _timer)
      {
	if (_mb.vcpustate(0)->hazard & VirtualCpuState::HAZARD_BIOS)
	  {
	    // a timeout happened -> howto return error code?
	    Logging::printf("BIOS disk timeout\n");
	    write_bda(DISK_COMPLETION_CODE, 1, 1);
	    Cpu::atomic_and<volatile unsigned>(&_mb.vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_BIOS);
	  }
	return true;
      }
    return false;
  };


  /**
   * The memory read routine for the last 16byte below 4G and below 1M.
   */
  bool receive(MessageMemRead &msg)
  {
    if (!in_range(msg.phys, 0xfffffff0, 0x10) && !in_range(msg.phys, 0xffff0, 0x10) )  return false;
    memcpy(msg.ptr, _resetvector + (msg.phys & 0xf), msg.count);
    return true;
  }


  /**
   * Handle messages from the keyboard host driver.
   */
  bool  receive(MessageKeycode &msg)
  {
    if (msg.keyboard == 0)
      {
	_lastkey = msg.keycode;
	unsigned value = keycode2bios(msg.keycode);
	unsigned short next  = read_bda(0x1a);
	unsigned short first = read_bda(0x1c);
	unsigned short start = read_bda(0x80);
	unsigned short end   = read_bda(0x82);

	first += 0x2;
	if (first >= end)   first = start;
	if (value && first != next)
	  {
	    write_bda(read_bda(0x1c), value, 2);
	    write_bda(0x1c, first, 2);
	  }
	return true;
      }
    Logging::printf("%s() ignored %x %x\n", __PRETTY_FUNCTION__, msg.keyboard, msg.keycode);
    return false;
  }

  /**
   * Execute BIOS code.
   */
  bool  receive(MessageExecutor &msg)
  {
    CpuState *cpu = msg.cpu;
    VirtualCpuState *vcpu = msg.vcpu;
    assert(cpu->head.pid == 33);

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
	  case 0x08:  return handle_int08(cpu);
	  case 0x09:  return handle_int09(cpu);
	  case 0x0f:  return do_iret(cpu);
	  case 0x11: // BIOS equipment word
	    cpu->ax = 0x34; // 80x25, ps2-mouse, no-floppy
	    return do_iret(cpu);
	  case 0x12: // get low memory
	    cpu->ax = read_bda(0x13);
	    return do_iret(cpu);
	  case 0x13:  return handle_int13(cpu, vcpu);
	  case 0x15:  return handle_int15(cpu);
	  case 0x16:  return handle_int16(cpu);
	  case 0x17:  // printer
	    cpu->efl |= 1;  // unsupported
	    return do_iret(cpu);
	  case 0x19:  return boot_from_disk(cpu, vcpu);
	  case 0x1c:
	    COUNTER_INC("int1c");
	    return do_iret(cpu);
	  case 0x1a:  return handle_int1a(cpu);
	  case RESET_VECTOR:
	    reset_helper();
	    // notify optionroms lately
	    _mb.bus_bios.send(msg2);
	    return jmp_int(cpu, 0x19);
	  case WAIT_DISK_VECTOR:
	    if (~vcpu->hazard & VirtualCpuState::HAZARD_BIOS)
	      {
		cpu->al = read_bda(DISK_COMPLETION_CODE);
		return do_iret(cpu);
	      }
	    return true;
	  default:
	    VB_UNIMPLEMENTED;
	  }
      }
    return false;
  }

  /**
   * Answer HostRequests from DummyHostDevices.
   */
  bool  receive(MessageHostOp &msg)
  {
    switch (msg.type)
      {
      case MessageHostOp::OP_ALLOC_IOIO_REGION:
      case MessageHostOp::OP_ALLOC_IOMEM:
      case MessageHostOp::OP_ATTACH_HOSTIRQ:
      case MessageHostOp::OP_ASSIGN_PCI:
	// we have all ports and irqs
	return true;
      case MessageHostOp::OP_NOTIFY_IRQ:
      case MessageHostOp::OP_VIRT_TO_PHYS:
      case MessageHostOp::OP_GUEST_MEM:
      case MessageHostOp::OP_ALLOC_FROM_GUEST:
      case MessageHostOp::OP_GET_MODULE:
      case MessageHostOp::OP_GET_UID:
      default:
	Logging::panic("%s - unimplemented operation %x", __PRETTY_FUNCTION__, msg.type);
      }
  };


  /**
   * Forward IO messages to the device models and vice-versa.
   */
  bool  receive(MessageIOIn &msg)  { return _mb.bus_ioin.send(msg); }
  bool  receive(MessageIOOut &msg) { return _mb.bus_ioout.send(msg); }
  bool  receive(MessageLegacy &msg) { return _hostmb->bus_legacy.send_fifo(msg); }


  VirtualBios(unsigned short segment, Motherboard &mb)  : BiosCommon(mb), _base(static_cast<unsigned>(segment) << 4)
    {
      MessageTimer msg0;
      if (!mb.bus_timer.send(msg0))
	Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);
      _timer = msg0.nr;

      MessageHostOp msg1(MessageHostOp::OP_GUEST_MEM, 0);
      if (!mb.bus_hostop.send(msg1))
	Logging::panic("%s can't get physical memory size", __PRETTY_FUNCTION__);
      _memsize = msg1.len;

      // create hostmb and hostkeyb
      _hostmb = new Motherboard(mb.clock());
      _hostmb->bus_keycode.add(this, &VirtualBios::receive_static<MessageKeycode>);
      _hostmb->bus_hostop.add(this, &VirtualBios::receive_static<MessageHostOp>);
      _hostmb->bus_hwioin.add(this, &VirtualBios::receive_static<MessageIOIn>);
      _hostmb->bus_hwioout.add(this, &VirtualBios::receive_static<MessageIOOut>);
      _mb.bus_legacy.add(this, &VirtualBios::receive_static<MessageLegacy>);

      char args[] = "hostkeyb:0,0x60,1,,1";
      _hostmb->parse_args(args);

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

      mb.bus_memread.add(this,     &VirtualBios::receive_static<MessageMemRead>);
      mb.bus_diskcommit.add(this,  &VirtualBios::receive_static<MessageDiskCommit>);
      mb.bus_timeout.add(this,     &VirtualBios::receive_static<MessageTimeout>);

      // get sectors of the disk
      MessageDisk msg2(0, &_disk_params);
      if (!_mb.bus_disk.send(msg2))
	{
	  _disk_params.flags = 0;
	  _disk_params.sectors = 0;
	}
    }
};

PARAM(vbios,
      {
	mb.bus_executor.add(new VirtualBios(argv[0], mb),  &VirtualBios::receive_static, 33);
      },
      "vbios:segment - create a executor that emulates a virtual bios.",
      "Example: 'vbios:0xf000'.");
