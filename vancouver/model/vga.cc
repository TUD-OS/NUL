/**
 * Basic VGA emulation.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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
#include "executor/bios.h"
#include "host/screen.h"

/**
 * A VGA compatible device.
 *
 * State: unstable
 * Features: textmode 80x25 supported, cursor
 * Missing: plenty, e.g: PCI, IRQ, VESA framebuffer...
 * Documentation: FreeVGA chipset reference - vga.htm
 */
class Vga : public StaticReceiver<Vga>, public BiosCommon
{
  static const unsigned SIZE = 0x8000;
  unsigned short _view;
  unsigned short _iobase;
  unsigned _textbase;
  char    *_ptr;
  VgaRegs _regs;
  unsigned char _crt_index;

  void debug_dump() {
    Device::debug_dump();
    Logging::printf("    iobase %x+32 textbase %x", _iobase, _textbase);
  };
  const char *debug_getname() { return "VGA"; };

 public:

  void putchar_guest(unsigned short value)
  {
    Screen::vga_putc(value, reinterpret_cast<unsigned short *>(_ptr), _regs.cursor_pos);
  }


  void puts_guest(const char *msg)
  {
    for (unsigned i=0; msg[i]; i++)  putchar_guest(0x0f00 | msg[i]);
  }


  bool handle_reset()
  {
    _regs.offset = 0;
    _regs.mode   = 0;
    _regs.cursor_pos  = 24*80*2;
    _regs.cursor_style = 0x0d0e;

    // clear screen
    for (unsigned i=0; i < 25; i++) putchar_guest('\n');
    puts_guest("    VgaBios booting...\n\n\n");
    write_bda(0x4a, 80, 1); // columns on screen
    write_bda(0x84, 24, 1); // rows - 1
    write_bda(0x85, 16, 1); // character height in scan-lines
    return true;
  }

  static unsigned vesa_farptr(CpuState *cpu, void *p, void *base)  {  
    return (cpu->es.sel << 16) |  (cpu->di + reinterpret_cast<char *>(p) - reinterpret_cast<char *>(base)); 
  }

  unsigned get_vesa_mode(unsigned vesa_mode, ConsoleModeInfo *info)
  {
    for (MessageConsole msg2(0, info); msg2.index < (Vbe::MAX_VESA_MODES - 1) && _mb.bus_console.send(msg2); msg2.index++)
      {
	if (vesa_mode == info->_vesa_mode) return msg2.index;
      }
    return ~0u;
  }

public:
  bool  handle_vesa(CpuState *cpu)
  {
    static const char *oemstring = "Vancouver VESA BIOS";
    switch (cpu->ax)
      {
      case 0x4f00: // vesa information
	{
	  Vbe::InfoBlock v;
	  copy_in(cpu->es.base + cpu->di, &v, sizeof(v));
	  Logging::printf("VESA %x tag %x base %x+%x esi %x\n", cpu->eax, v.tag, cpu->es.base, cpu->di, cpu->esi);

	  // we support VBE 2.0
	  v.version = 0x0201;

	  // add ptr to scratch area
	  char *p = v.scratch;
	  strcpy(p, oemstring);	 
	  v.oem_string = vesa_farptr(cpu, p, &v);
	  p += strlen(p) + 1;

	  v.caps = 0;
	  unsigned short *modes = reinterpret_cast<unsigned short *>(p);
	  v.video_mode_ptr = vesa_farptr(cpu, modes, &v);

	  // get all modes
	  ConsoleModeInfo info;
	  for (MessageConsole msg2(0, &info); msg2.index < (Vbe::MAX_VESA_MODES - 1) && _mb.bus_console.send(msg2); msg2.index++)
	    *modes++ = info._vesa_mode;
	  *modes++ = 0xffff;

	  // report 4M as framebuffer
	  v.memory = (4<<20) >> 16;
	  if (v.tag == Vbe::TAG_VBE2)
	    {
	      v.oem_revision = 0;
	      v.oem_vendor = 0;
	      v.oem_product = 0;
	      v.oem_product_rev = 0;
	    }
	  v.tag = Vbe::TAG_VESA;
	  copy_out(cpu->es.base + cpu->di, &v, sizeof(v));
	}
	break;
      case 0x4f01: // get modeinfo
	{
	  Logging::printf("VESA %x base %x+%x esi %x size %x\n", cpu->eax, cpu->es.base, cpu->di, cpu->esi, sizeof(ConsoleModeInfo));
	  
	  ConsoleModeInfo info;	 
	  if (get_vesa_mode((cpu->eax >> 16), &info) != 0ul)
	    {
	      info.phys_base = 0x100000; // XXX
	      copy_out(cpu->es.base + cpu->di, &info, sizeof(info));
	      break;
	    }
	}
	return false;
      case 0x4f02: // set vbemode
	{
	  ConsoleModeInfo info;
	  unsigned index = get_vesa_mode(cpu->ebx & 0x0fff, &info);
	  if (index != ~0u)
	    {
	      // ok, we have the mode -> set it
	      Logging::printf("VESA %x base %x+%x esi %x mode %x\n", cpu->eax, cpu->es.base, cpu->di, cpu->esi, index);
	      _regs.mode =  index;
	      // XXX clear the shadow framebuffer if flag is set
	      break;
	    }
	}
      case 0x4f15: // DCC
      default:
	return false;
      }
    cpu->ax = 0x004f;
    return true;
  }


  /**
   * Graphic INT.
   */
  bool handle_int10(CpuState *cpu)
  {
    COUNTER_INC("int10");
    switch (cpu->ah)
      {
      case 0x00: // set mode
	// unsupported
	//DEBUG;
	break;
      case 0x01: // set cursor shape
	_regs.cursor_style = cpu->cx;
	break;
      case 0x02: // set cursor
	// we support only a single page
	_regs.cursor_pos = ((cpu->dx >> 8)*80 + (cpu->dx & 0xff))*2;
	break;
      case 0x03: // get cursor
	cpu->ax = 0;
	cpu->cx = _regs.cursor_style;
	cpu->dx = ((_regs.cursor_pos / 160) << 8) + ((_regs.cursor_pos / 2) % 80);
	break;
      case 0x09: // write char+attr
      case 0x0a: // write char only
	{
	  unsigned short value = (cpu->bl << 8) | cpu->al;
	  for (unsigned i=0; i < cpu->cx; i++)
	    putchar_guest(value);
	}
	break;
      case 0x0e: // write char - teletype output
	putchar_guest(0x0f00 | cpu->al);
	break;
      case 0x0f: // get video mode
	cpu->ax = 0x5003; // 80 columns, 80x25 16color mode
	cpu->bh = 0;      // 0 is the active page
	break;
      case 0x12:
	switch (cpu->bl)
	  {
	  case 0x10:  // get ega info
	    cpu->bx = 0x0000;  //  color mode, 64 kb
	    cpu->cx = 0x0007;  //  0-features, mode: 7 (80x25)
	    break;
	  case 0x01: // unkown windows boot
	    DEBUG;
	    break;
	  default:
	    VB_UNIMPLEMENTED;
	  }
	break;
      default:
	switch (cpu->ax)
	  {
	  case 0x1130:        // get font information
	    // XXX fake the pointers?
	    #if 1
	    cpu->es.sel      = 0xc000;
	    cpu->es.base     = 0xc0000;
	    cpu->bp          = cpu->bx*100;
	    #endif
	    cpu->cx = read_bda(0x85) & 0xff;
	    cpu->dl = read_bda(0x84);
	    break;
	  case 0x1a00:        // display combination code
	    cpu->al = 0x1a;   // function supported
	    cpu->bx = 0x0008; // vga color active
	    break;
	  case 0x2000: // unknown during windows boot
	    cpu->efl |= 1;
	    // unsupported
	    break;
	  default:
	    if (!handle_vesa(cpu))
	      VB_UNIMPLEMENTED;
	  }
      }
    return true;
  }


  bool  receive(MessageBios &msg)
  {
    switch(msg.irq)
      {
      case 0x10: return handle_int10(msg.cpu);
      case RESET_VECTOR: return handle_reset();
      default:
	return false;
      }
  }


  bool  receive(MessageIOOut &msg)
  {
    bool res = false;
    for (unsigned i = 0; i < (1u << msg.type); i++)
      {
	unsigned char value = msg.value >> i*8;
	if (in_range(msg.port + i, _iobase, 32))
	  {
	    switch (msg.port + i - _iobase)
	      {
	      case 0x0: // attribute address and write
	      case 0x1: // attribute read
	      case 0x8: // dac address write mode
	      case 0x9: // dac data
	      case 0xe: // graphics controller address
	      case 0xf: // graphics controller data
		break;
	      case 0x14: // crt address
		_crt_index = value;
		break;
	      case 0x15: // crt data
		switch (_crt_index)
		  {
		  case 0x0a: // cursor scanline start
		    _regs.cursor_style = (value << 8) | (_regs.cursor_style & 0xff);
		    break;
		  case 0x0b: // cursor scanline end
		    _regs.cursor_style = (_regs.cursor_style & ~0xff) | value;
		    break;
		  case 0x0e: // cursor location high
		    _regs.cursor_pos = (value << 8) | (_regs.cursor_pos & 0xff);
		    break;
		  case 0x0f: // cursor location low
		    _regs.cursor_pos = (_regs.cursor_pos & ~0xff) | value;
		    break;
		  case 0x0c: // start address high
		    _regs.offset = (value << 8) | (_regs.offset & 0xff);
		    break;
		  case 0x0d: // start address low
 		    _regs.offset = (_regs.offset & ~0xff) | value;
		    break;
		  default:
		    Logging::printf("%s ignore crt register %x\n", __PRETTY_FUNCTION__, _crt_index);
		  };
		break;
	      default:
		Logging::printf("%s ignore register %x\n", __PRETTY_FUNCTION__, msg.port + i - _iobase);
	      }
	    res = true;
	  }
      }
    return res;
  }


  bool  receive(MessageIOIn &msg)
  {
    bool res = false;;
    for (unsigned i = 0; i < (1u << msg.type); i++)
      {
	if (in_range(msg.port + i, _iobase, 32))
	  {
	    unsigned char value = ~0;
	    switch (msg.port + i - _iobase)
	      {
	      case 0x14: // crt address
		value = _crt_index;
		break;
	      case 0x13: // alias of crt data
	      case 0x15: // crt data
		switch (_crt_index)
		  {
		  case 0x0a: // cursor scanline start
		    value = _regs.cursor_style >> 8;
		    break;
		  case 0x0b: // cursor scanline end
		    value = _regs.cursor_style;
		    break;
		  case 0x0e: // cursor location high
		    value = _regs.cursor_pos >> 8;
		    break;
		  case 0x0f: // cursor location low
		    value = _regs.cursor_pos;
		    break;
		  case 0x0c: // start addres high
		    value = _regs.offset >> 8;
		    break;
		  case 0x0d: // start addres low
		    value = _regs.offset;
		    break;
		  default:
		    Logging::printf("%s ignore crt register %x\n", __PRETTY_FUNCTION__, _crt_index);
		  }
		break;
	      default:
		Logging::printf("%s ignore register %x\n", __PRETTY_FUNCTION__, msg.port + i - _iobase);
	      }
	    msg.value = (msg.value & ~(0xff << i*8)) | (value << i*8);
	    res = true;
	  }
      }
    return res;
  }


  bool  receive(MessageMemRead &msg)
  {
    if (!in_range(msg.phys,_textbase, SIZE - msg.count))  return false;
    memcpy(msg.ptr, _ptr + msg.phys - _textbase, msg.count);
    return true;
  }


  bool  receive(MessageMemWrite &msg)
  {
    if (!in_range(msg.phys,_textbase, SIZE - msg.count))  return false;
    memcpy(_ptr + msg.phys - _textbase, msg.ptr, msg.count);
    return true;
  }


  bool  receive(MessageMemMap &msg)
  {
    if (!in_range(msg.phys,_textbase, SIZE))  return false;
    msg.phys = _textbase;
    msg.ptr = _ptr;
    msg.count = SIZE;
    return true;
  }



  Vga(Motherboard &mb, unsigned short iobase, unsigned long textbase)
    : BiosCommon(mb), _iobase(iobase), _textbase(textbase), _crt_index(0)
  {
    // alloc console
    _ptr = reinterpret_cast<char *>(memalign(0x1000, SIZE));
    memset(_ptr, 0, SIZE);
    MessageConsole msg("VM", _ptr, SIZE, &_regs);
    if (!_ptr || !mb.bus_console.send(msg))
      Logging::panic("could not alloc a VGA backend");
    _view = msg.view;
    Logging::printf("VGA console %lx %p\n", textbase, _ptr);

    // switch to our console
    msg.type = MessageConsole::TYPE_SWITCH_VIEW;
    mb.bus_console.send(msg);
  }
};


PARAM(vga,
      {
	Device *dev = new Vga(mb, argv[0], argv[1]);
	mb.bus_ioin.  add(dev, &Vga::receive_static<MessageIOIn>);
	mb.bus_ioout. add(dev, &Vga::receive_static<MessageIOOut>);
	mb.bus_bios.  add(dev, &Vga::receive_static<MessageBios>);
	mb.bus_memread.add(dev, &Vga::receive_static<MessageMemRead>);
	mb.bus_memwrite.add(dev, &Vga::receive_static<MessageMemWrite>);
	mb.bus_memmap.add(dev, &Vga::receive_static<MessageMemMap>);
      },
      "vga:iobase,memtext - attach a VGA controller in text mode 80x25.",
      "Example: 'vga:0x3c0,0xb8000'",
      "This also adds support for VGA graphics bios support");
