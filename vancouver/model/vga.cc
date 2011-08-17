/** @file
 * Basic VGA emulation.
 *
 * Copyright (C) 2007-2010, Bernhard Kauer <bk@vmmon.org>
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

#include "nul/motherboard.h"
#include "executor/bios.h"
#include "host/screen.h"

/**
 * A VGA compatible device.
 *
 * State: unstable
 * Features: textmode 80x25 supported, cursor, VESA framebuffer
 * Missing: plenty, e.g: PCI, IRQ, many BIOS functions, 8-bit graphic modes
 * Documentation: FreeVGA chipset reference - vga.htm, Browns IRQ List
 */
class Vga : public StaticReceiver<Vga>, public BiosCommon
{
  enum {
    LOW_BASE  = 0xa0000,
    LOW_SIZE  = 1<<17,
    TEXT_OFFSET = 0x18000 >> 1,
    EBDA_FONT_OFFSET = 0x1000,
  };
  unsigned short _view;
  unsigned short _iobase;
  char         * _framebuffer_ptr;
  unsigned long  _framebuffer_phys;
  unsigned long  _framebuffer_size;
  VgaRegs        _regs;
  unsigned char  _crt_index;
  unsigned       _ebda_segment;
  unsigned       _vbe_mode;

  void puts_guest(const char *msg) {
    unsigned pos = _regs.cursor_pos - TEXT_OFFSET;
    for (unsigned i=0; msg[i]; i++)
      Screen::vga_putc(0x0f00 | msg[i], reinterpret_cast<unsigned short *>(_framebuffer_ptr) + TEXT_OFFSET, pos);
    update_cursor(0, ((pos / 80) << 8) | (pos % 80));
  }


  /**
   * Update the cursor of a page and sync the hardware cursor with the
   * one of the active page.
   */
  void update_cursor(unsigned page, unsigned pos) {
    write_bda(0x50 +  (page & 0x7) * 2, pos, 2);
    pos = read_bda(0x50 + 2 * (read_bda(0x62) & 0x7));
    _regs.cursor_pos = TEXT_OFFSET + ((pos >> 8)*80 + (pos & 0xff));
  }


  /**
   * Get the page offset.
   */
  unsigned get_page(unsigned page) { return 0x800 * (page & 0x7); }

  /**
   * Return the text mode cursor position in characters for a given page.
   */
  unsigned get_pos(unsigned page) {
    unsigned res = read_bda(0x50 + (page & 0x7) * 2);
    return (res >> 8)*80 + (res & 0xff);
  }


  bool handle_reset(bool show)
  {
    _regs.offset       = TEXT_OFFSET;
    _regs.mode         = 0;
    _regs.cursor_pos   = 24*80 + TEXT_OFFSET;
    _regs.cursor_style = 0x0d0e;
    // and clear the screen
    memset(_framebuffer_ptr, 0, _framebuffer_size);
    if (show) puts_guest("    VgaBios booting...\n\n\n");
    return true;
  }

  static unsigned vesa_farptr(CpuState *cpu, void *p, void *base)  {
    return (cpu->es.sel << 16) |  (cpu->di + reinterpret_cast<char *>(p) - reinterpret_cast<char *>(base));
  }

  unsigned get_vesa_mode(unsigned vesa_mode, ConsoleModeInfo *info)
  {
    for (MessageConsole msg2(0, info); (msg2.index < (Vbe::MAX_VESA_MODES - 1)) && _mb.bus_console.send(msg2); msg2.index++)
      {
	if (vesa_mode == info->_vesa_mode)
	  {
	    // fix memory info
	    unsigned long image_size = info->bytes_per_scanline * info->resolution[1];
	    if (!image_size || image_size > _framebuffer_size)
	      info->attr &= ~1u;
	    else
	      {
		unsigned image_pages = (_framebuffer_size / image_size) - 1;
		if (image_pages > 0xff) image_pages = 0xff;
		info->number_images     = image_pages;
		info->number_images_bnk = image_pages;
		info->number_images_lin = image_pages;
	      }
	    return msg2.index;
	  }
      }
    return ~0u;
  }

  bool  handle_vesa(CpuState *cpu)
  {
    static const char *oemstring = "Vancouver VESA BIOS";
    switch (cpu->ax)
      {
      case 0x4f00: // vesa information
	{
	  Vbe::InfoBlock v;
	  // clear the block
	  memset(&v, 0, sizeof(v));

	  // copy in the tag
	  copy_in(cpu->es.base + cpu->di, &v, 4);
	  Logging::printf("VESA %x tag %x base %x+%x esi %x\n", cpu->eax, v.tag, cpu->es.base, cpu->di, cpu->esi);

	  // we support VBE 2.0
	  v.version = 0x0200;

	  unsigned short *modes = reinterpret_cast<unsigned short *>(v.scratch);
	  v.video_mode_ptr = vesa_farptr(cpu, modes, &v);

	  // get all modes
	  ConsoleModeInfo info;
	  for (MessageConsole msg2(0, &info); msg2.index < (Vbe::MAX_VESA_MODES - 1) && _mb.bus_console.send(msg2); msg2.index++)
	    *modes++ = info._vesa_mode;
	  *modes++ = 0xffff;

	  // set the oemstring
	  char *p = reinterpret_cast<char *>(modes);
	  strcpy(p, oemstring);
	  v.oem_string = vesa_farptr(cpu, p, &v);
	  p += strlen(p) + 1;
	  assert (p < reinterpret_cast<char *>((&v)+1));

	  v.memory = _framebuffer_size >> 16;
	  unsigned copy_size = (v.tag == Vbe::TAG_VBE2) ? sizeof(v) : 256;
	  v.tag = Vbe::TAG_VESA;
	  copy_out(cpu->es.base + cpu->di, &v, copy_size);
	}
	break;
      case 0x4f01: // get modeinfo
	{
	  ConsoleModeInfo info;
	  if (get_vesa_mode(cpu->ecx & 0x0fff, &info) != 0ul)
	    {
	      info.phys_base = _framebuffer_phys;
	      copy_out(cpu->es.base + cpu->di, &info, sizeof(info));
	      break;
	    }
	}
	cpu->ax = 0x024f;
	return true;
      case 0x4f02: // set vbemode
	{
	  ConsoleModeInfo info;
	  unsigned index = get_vesa_mode(cpu->ebx & 0x0fff, &info);
	  if (index != ~0u && info.attr & 1)
	    {
	      // ok, we have the mode -> set it
	      Logging::printf("VESA %x base %x+%x esi %x mode %x\n", cpu->eax, cpu->es.base, cpu->di, cpu->esi, index);

	      // clear buffer
	      if (~cpu->ebx & 0x8000)  memset(_framebuffer_ptr, 0, _framebuffer_size);

	      // switch mode
	      _regs.mode =  index;
	      _vbe_mode = cpu->ebx;
	      break;
	    }
	  cpu->ax = 0x024f;
	  return true;
	}
      case 0x4f03: // get vbemode
	cpu->bx = _vbe_mode;
	break;
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
  bool handle_int10(MessageBios &msg)
  {
    CpuState *cpu = msg.cpu;
    //DEBUG(cpu);
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
	update_cursor(cpu->bh, cpu->dx);
	break;
      case 0x03: // get cursor
	  cpu->ax = 0;
	  cpu->cx = _regs.cursor_style;
	  cpu->dx = read_bda(0x50 + (cpu->bh & 0x7) * 2);
	break;
      case 0x05: // set current page
	write_bda(0x62, cpu->al & 7, 1);
	_regs.offset = TEXT_OFFSET + get_page(cpu->al);
	break;
      case 0x06: // scroll up window
	{
	  unsigned char current_page = read_bda(0x62);
	  unsigned short *base = reinterpret_cast<unsigned short *>(_framebuffer_ptr) + TEXT_OFFSET + get_page(current_page);
	  unsigned rows = (cpu->al == 0) ? 25 : cpu->al;
	  unsigned maxrow = cpu->dh < 25 ? cpu->dh : 24;
	  for (unsigned row = cpu->ch; row <= maxrow; row++)
	    for (unsigned col = cpu->cl; col < 80 && col <= cpu->dl; col++)
	      if ((row + rows) > maxrow)
		base[row*80 + col] = cpu->bh << 8;
	      else
		base[row*80 + col] = base[(row + rows)*80 + col];
	}
	break;
      case 0x08: // read character attributes
	{
	  unsigned page = get_page(cpu->bh);
	  unsigned pos  = get_pos(cpu->bh);
	  cpu->ax = *(reinterpret_cast<unsigned short *>(_framebuffer_ptr) + TEXT_OFFSET + page + pos);
	}
	break;
      case 0x09: // write char+attr
      case 0x0a: // write char only
	{
	  unsigned page = get_page(cpu->bh);
	  unsigned pos = get_pos(cpu->bh);
	  for (unsigned i=0; i < cpu->cx; i++) {
	    unsigned offset = page + pos + i;

	    // check for overflow
	    if (offset < 0x800*8) {
		if (cpu->ah & 1) _framebuffer_ptr[2*(TEXT_OFFSET + offset) + 1] = cpu->bl;
		_framebuffer_ptr[2*(TEXT_OFFSET + offset) + 0] = cpu->al;
	    }
	  }
	}
	break;
      case 0x0e: // write char - teletype output
	{
	  unsigned page  = get_page(cpu->bh);
	  unsigned pos   = get_pos(cpu->bh);
	  unsigned value = ((_framebuffer_ptr[2*(TEXT_OFFSET + page + pos) + 1] & 0xff) << 8);

	  value |= cpu->al;
	  Screen::vga_putc(value, reinterpret_cast<unsigned short *>(_framebuffer_ptr) + TEXT_OFFSET + page, pos);
	  update_cursor(cpu->bh, ((pos / 80) << 8) | (pos % 80));
	}
	break;
      case 0x0f: // get video mode
	cpu->ax = read_bda(0x49);
	cpu->bh = read_bda(0x62);
	break;
      case 0x12:
	switch (cpu->bl)
	  {
	  case 0x10:  // get ega info
	    cpu->bx = 0x0000;  //  color mode, 64 kb
	    cpu->cx = 0x0007;  //  0-features, mode: 7 (80x25)
	    break;
	  case 0x01: // unknown windows boot
	    DEBUG(cpu);
	    break;
	  default:
	    DEBUG(cpu);
	  }
	break;
      default:
	switch (cpu->ax)
	  {
	  case 0x1130:        // get font information
	    switch (cpu->bh) {
	    case 0:
	      cpu->es.sel   = read_bda(4*0x1f);
	      cpu->bp       = cpu->es.sel >> 16;
	      cpu->es.sel  &= 0xffff;
	      cpu->es.base  = cpu->es.sel << 4;
	      break;
	    case 1:
	      cpu->es.sel   = read_bda(4*0x43);
	      cpu->bp       = cpu->es.sel >> 16;
	      cpu->es.sel  &= 0xffff;
	      cpu->es.base  = cpu->es.sel << 4;
	      break;
	    case 5 ... 7:
	      cpu->es.sel      = _ebda_segment;
	      cpu->es.base     = cpu->es.sel << 4;
	      cpu->bp          = EBDA_FONT_OFFSET;
	      // we let the alternate tables start just before the
	      // font, as this byte would be zero, we are fine
	      if (cpu->bh == 7 || cpu->bh == 5) cpu->bp--;
	      break;
	    default:
	      DEBUG(cpu);
	    }
	    DEBUG(cpu);
	    cpu->cx = read_bda(0x85) & 0xff;
	    cpu->dl = read_bda(0x84);
	    msg.mtr_out |= MTD_DS_ES | MTD_GPR_BSD;
	    break;
	  case 0x1a00:        // display combination code
	    cpu->al = 0x1a;   // function supported
	    cpu->bx = 0x0008; // vga color active
	    break;
	  case 0x2000: // unknown during windows boot
	    cpu->efl |= 1;
	    msg.mtr_out |= MTD_RFLAGS;
	    // unsupported
	    break;
	  default:
	    if (!handle_vesa(cpu))
	      DEBUG(cpu);
	  }
      }
    //DEBUG(cpu);
    msg.mtr_out |= MTD_GPR_ACDB;
    return true;
  }

public:

  bool  receive(MessageBios &msg)
  {
    switch(msg.irq)
      {
      case 0x10: return handle_int10(msg);
      case RESET_VECTOR: return handle_reset(true);
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
		    _regs.cursor_pos = TEXT_OFFSET + ((value << 8) | (_regs.cursor_pos & 0xff));
		    break;
		  case 0x0f: // cursor location low
		    _regs.cursor_pos = (_regs.cursor_pos & ~0xff) | value;
		    break;
		  case 0x0c: // start address high
		    _regs.offset = TEXT_OFFSET + ((value << 8) | (_regs.offset & 0xff));
		    break;
		  case 0x0d: // start address low
 		    _regs.offset = (_regs.offset & ~0xff) | value;
		    break;
		  default:
		    break;
		  };
		break;
	      default:
		break;
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
		    value = (_regs.cursor_pos - TEXT_OFFSET) >> 8;
		    break;
		  case 0x0f: // cursor location low
		    value = (_regs.cursor_pos - TEXT_OFFSET);
		    break;
		  case 0x0c: // start addres high
		    value = (_regs.offset - TEXT_OFFSET) >> 8;
		    break;
		  case 0x0d: // start addres low
		    value = _regs.offset;
		    break;
		  default:
		    break;
		  }
		break;
	      default:
		break;
	      }
	    msg.value = (msg.value & ~(0xff << i*8)) | (value << i*8);
	    res = true;
	  }
      }
    return res;
  }


  bool  receive(MessageMem &msg)
  {
    unsigned *ptr;
    if (in_range(msg.phys, _framebuffer_phys, _framebuffer_size))
      ptr = reinterpret_cast<unsigned *>(_framebuffer_ptr + msg.phys - _framebuffer_phys);
    else if (in_range(msg.phys, LOW_BASE, LOW_SIZE))
      ptr = reinterpret_cast<unsigned *>(_framebuffer_ptr + msg.phys - LOW_BASE);
    else return false;

    if (msg.read) *msg.ptr = *ptr; else *ptr = *msg.ptr;
    return true;
  }


  bool  receive(MessageMemRegion &msg)
  {
    if (in_range(msg.page, _framebuffer_phys >> 12, _framebuffer_size >> 12)) {
      msg.start_page = _framebuffer_phys >> 12;
      msg.count = _framebuffer_size >> 12;
    }
    else if (in_range(msg.page, LOW_BASE >> 12, LOW_SIZE >> 12)) {
	msg.start_page = LOW_BASE >> 12;
	msg.count = LOW_SIZE >> 12;
    }
    else return false;
    msg.ptr = _framebuffer_ptr;
    return true;
  }

  bool  receive(MessageDiscovery &msg) {
    if (msg.type != MessageDiscovery::DISCOVERY) return false;
    discovery_write_dw("bda",  0x49,    3, 1); // current videomode
    discovery_write_dw("bda",  0x4a,   80, 1); // columns on screen
    discovery_write_dw("bda",  0x4c, 4000, 2); // screenbytes: 80*25*2
    for (unsigned i=0; i < 8; i++)             // cursor positions
      discovery_write_dw("bda",  0x50 + 2*i,    0, 2);
    discovery_write_dw("bda",  0x84,   24, 1); // rows - 1
    discovery_write_dw("bda",  0x85,   16, 1); // character height in scan-lines
    discovery_write_dw("bda",  0x62,    0, 1); // current page address
    discovery_write_dw("bda",  0x63, _iobase + 0x14, 2); // crt address


    MessageConsole msg2(MessageConsole::TYPE_GET_FONT);
    msg2.ptr = _framebuffer_ptr;
    if (_mb.bus_console.send(msg2)) {
      // write it to the EBDA
      discovery_write_st("ebda", EBDA_FONT_OFFSET, _framebuffer_ptr, 0x1000);
      discovery_read_dw("bda", 0xe, _ebda_segment);
      // set font vector
      discovery_write_dw("realmode idt", 0x43 * 4, (_ebda_segment << 16) | EBDA_FONT_OFFSET);
    }
    return true;
  }


  Vga(Motherboard &mb, unsigned short iobase, char *framebuffer_ptr, unsigned long framebuffer_phys, unsigned long framebuffer_size)
    : BiosCommon(mb), _iobase(iobase), _framebuffer_ptr(framebuffer_ptr), _framebuffer_phys(framebuffer_phys), _framebuffer_size(framebuffer_size), _crt_index(0)
  {
    assert(!(framebuffer_phys & 0xfff));
    assert(!(framebuffer_size & 0xfff));

    handle_reset(false);


    // alloc console
    MessageConsole msg("VM", _framebuffer_ptr, _framebuffer_size, &_regs);
    if (!mb.bus_console.send(msg))
      Logging::panic("could not alloc a VGA backend");
    _view = msg.view;
    Logging::printf("VGA console %lx+%lx %p\n", _framebuffer_phys, _framebuffer_size, _framebuffer_ptr);

    // switch to our console
    msg.type = MessageConsole::TYPE_SWITCH_VIEW;
    mb.bus_console.send(msg);
  }
};

static unsigned long _default_vga_fbsize = 128;
PARAM_HANDLER(vga_fbsize,
	      "vga_fbsize:size - override the default fbsize for the 'vga' parameter (in KB)")
{
  _default_vga_fbsize = argv[0];
}

PARAM_HANDLER(vga,
	      "vga:iobase,fbsize=128 - attach a virtual VGA controller.",
	      "Example: 'vga:0x3c0,4096'",
	      "The framebuffersize is given in kilobyte and the minimum is 128k.",
	      "This also adds support for VGA and VESA graphics BIOS.")
{
  unsigned long fbsize = argv[1];
  if (fbsize == ~0ul) fbsize = _default_vga_fbsize;

  // We need at least 128k for 0xa0000-0xbffff.
  if (fbsize   < 128)  fbsize = 128;
  fbsize <<= 10;
  MessageHostOp msg(MessageHostOp::OP_ALLOC_FROM_GUEST, fbsize);
  MessageHostOp msg2(MessageHostOp::OP_GUEST_MEM, 0UL);
  if (!mb.bus_hostop.send(msg) || !mb.bus_hostop.send(msg2))
    Logging::panic("%s failed to alloc %ld from guest memory\n", __PRETTY_FUNCTION__, fbsize);

  Vga *dev = new Vga(mb, argv[0], msg2.ptr + msg.phys, msg.phys, fbsize);
  mb.bus_ioin     .add(dev, Vga::receive_static<MessageIOIn>);
  mb.bus_ioout    .add(dev, Vga::receive_static<MessageIOOut>);
  mb.bus_bios     .add(dev, Vga::receive_static<MessageBios>);
  mb.bus_mem      .add(dev, Vga::receive_static<MessageMem>);
  mb.bus_memregion.add(dev, Vga::receive_static<MessageMemRegion>);
  mb.bus_discovery.add(dev, Vga::receive_static<MessageDiscovery>);
}

