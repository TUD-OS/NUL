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
	  Vbe::InfoBlock v;
	  copy_in(cpu->es.base + cpu->di, &v, sizeof(v));
	  Logging::printf("VESA %x tag %x base %x+%x esi %x\n", cpu->eax, v.tag, cpu->es.base, cpu->di, cpu->esi);

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
	  for (MessageConsole msg2(0, &info); _mb.bus_console.send(msg2); msg2.index++)
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
	  
	  // search whether we have the mode
	  ConsoleModeInfo info;
	  MessageConsole msg2(0, &info);
	  while (_mb.bus_console.send(msg2))
	    {
	      if (info._vesa_mode == (cpu->eax >> 16))
		break;
	      msg2.index++;
	    }


	  if (info._vesa_mode == (cpu->eax >> 16))
	    {
	      // XXX physbase
	      copy_out(cpu->es.base + cpu->di, &info, sizeof(info));
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
