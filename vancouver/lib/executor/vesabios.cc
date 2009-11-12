/**
 * Vesa Bios executor.
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
#include "vmm/motherboard.h"
#include "executor/bios.h"

/**
 * Provide a VESA BIOS.
 *
 * State: unstable
 * Missing: mode list, mode switching
 */
class VesaBios : public StaticReceiver<VesaBios>, public  BiosCommon 
{
  const char* debug_getname() { return "VesaBios"; }
  enum {
    TAG_VBE2 = 0x32454256,
    TAG_VESA = 0x41534556,
  };

  static unsigned vesa_farptr(CpuState *cpu, void *p, void *base)  {  
    return (cpu->es.sel << 16) |  (cpu->di + reinterpret_cast<char *>(p) - reinterpret_cast<char *>(base)); 
  }
public:
  bool  receive(MessageExecutor &msg)
  {
    static const char *oemstring = "Vancouver VESA BIOS";
    CpuState *cpu = msg.cpu;
    switch (cpu->ax)
      {
      case 0x4f00: // vesa information
	{
	  struct vbe_info {
	    unsigned tag;
	    unsigned short version;
	    unsigned __attribute__((packed)) oem;
	    unsigned __attribute__((packed)) cap;
	    unsigned __attribute__((packed)) modes;
	    unsigned short mem;
	    unsigned short revnr;
	    unsigned __attribute__((packed)) vendor;
	    unsigned __attribute__((packed)) product;
	    unsigned __attribute__((packed)) revision;
	    char scratch[224+256];
	  } v;
	  copy_in(cpu->es.base + cpu->di, &v, sizeof(v));
	  Logging::printf("VESA %x tag %x base %x+%x esi %x\n", cpu->eax, v.tag, cpu->es.base, cpu->di, cpu->esi);

	  v.version = 0x0201;

	  // add ptr to scratch area
	  char *p = v.scratch;
	  strcpy(p, oemstring);	 
	  v.oem = vesa_farptr(cpu, p, &v);
	  p+= strlen(p) + 1;

	  v.cap = 0;
	  unsigned short *modes = reinterpret_cast<unsigned short *>(p);
	  v.modes = vesa_farptr(cpu, modes, &v);

	  // get all modes
	  ConsoleModeInfo info;
	  for (MessageConsole msg2(0, &info); _mb.bus_console.send(msg2); msg2.index++)
	    *modes++ = info.vesamode;
	  *modes++ = 0xffff;
	  v.mem = 0;
	  if (v.tag == TAG_VBE2)
	    {
	      v.revnr = 0;
	      v.vendor = 0;
	      v.product = 0;
	      v.revision = 0;
	    }
	  v.tag = TAG_VESA;
	  copy_out(cpu->es.base + cpu->di, &v, sizeof(v));
	}
	break;
      case 0x4f01: // get modeinfo
	{
	  struct vbe_mode_info {
	    unsigned short attr;
	    unsigned short res[7];
	    unsigned short bytes_per_scanline;
	    unsigned short resolution[2];
	    unsigned char  res2[3];
	    unsigned char  bpp;
	    unsigned char  banks;
	    unsigned char  memory_mode;
	    unsigned char  res3[3];
	    unsigned char  masks[8];
	    unsigned char  direct_color_info;
	    // vbe2
	    unsigned physbase;
	    unsigned res4;
	    unsigned short res5;
	    // vbe3
	    unsigned char vbe3[16];
	    char scratch[189];
	  } __attribute__((packed)) v;
	  
	  Logging::printf("VESA %x base %x+%x esi %x size %x\n", cpu->eax, cpu->es.base, cpu->di, cpu->esi, sizeof(v));
	  
	  // search whether we have the mode
	  ConsoleModeInfo info;
	  MessageConsole msg2(0, &info);
	  while (_mb.bus_console.send(msg2))
	    {
	      if (info.vesamode == (cpu->eax >> 16))
		break;
	      msg2.index++;
	    }


	  if (info.vesamode == (cpu->eax >> 16))
	    {
	      memset(&v, 0, sizeof(v));
	      v.attr = 0x99;
	      v.bytes_per_scanline = info.bytes_per_scanline;
	      v.resolution[0] = info.resolution[0];
	      v.resolution[1] = info.resolution[1];
	      v.bpp = info.bpp;
	      v.memory_mode = 6; // direct color
	      // XXX physbase, masks, modeinfo
	      copy_out(cpu->es.base + cpu->di, &v, sizeof(v));
	      break;
	    }
	}
	return false;
      case 0x4f02: // set vbemode
	Logging::printf("VESA %x base %x+%x esi %x\n", cpu->eax, cpu->es.base, cpu->di, cpu->esi);
      case 0x4f15: // DCC
      default:
	return false;
      }
    cpu->ax = 0x004f;
    return true;
  }


  VesaBios(Motherboard &mb) : BiosCommon(mb) {}
};

PARAM(vesa,
      {
	mb.bus_bios.add(new VesaBios(mb),  &VesaBios::receive_static, 0x10);
      },
      "vesa - create a bios extension that emulates a virtual vesa bios.",
      "Example: 'vesa'.",
      "Please note that this component can not work standalone but needs a vbios to get its requests.");
