/**
 * Host VESA console driver.
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
#include "vmm/vesa.h"
#include "../executor/novainstcache.h"

using namespace Nova;

/**
 * A Vesa console.
 *
 * State: unstable
 * Features: 
 * Missing:  vesa realmode execution, get modelist
 */
class HostVesa : public StaticReceiver<HostVesa>
{
  enum {
    SS_SEG  = 0x1000,
    ES_SEG0 = 0x2000,
    ES_SEG1 = 0x3000,
    TIMER_NR = 2,
  };

  Motherboard  & _hostmb;
  Motherboard    _mb;
  char         * _mem;
  unsigned long  _memsize;
  CpuState       _cpu;
  timevalue      _timeout;
  unsigned short _modecount;
  VesaModeInfo * _modelist;
  unsigned       _instructions;
  bool           _vbe3;
  bool           _debug;
  
  const char *debug_getname() { return "HostVesa"; };


  bool vbe_call(unsigned eax, unsigned short es_seg, unsigned ecx=0, unsigned edx=0, unsigned ebx=0)
  {
    memset(&_cpu,  0, sizeof(_cpu));
    _cpu.cr0      = 0x10;
    _cpu.cs.ar    = 0x9b;
    _cpu.cs.limit = 0xffff;
    _cpu.ss.ar    = 0x93;
    _cpu.ds.ar = _cpu.es.ar = _cpu.fs.ar = _cpu.gs.ar = _cpu.ss.ar;
    _cpu.ld.ar    = 0x1000;
    _cpu.tr.ar    = 0x8b;
    _cpu.ss.limit = _cpu.ds.limit = _cpu.es.limit = _cpu.fs.limit = _cpu.gs.limit = _cpu.cs.limit;
    _cpu.tr.limit = _cpu.ld.limit = _cpu.gd.limit = _cpu.id.limit = 0xffff;
    _cpu.head.mtr = MTD_ALL;
    _cpu.dr7      = 0x400;

    // copy iret frame as well as HLT instruction to be able to come back
    _cpu.ss.sel  = SS_SEG;
    _cpu.ss.base = SS_SEG << 4;
    _cpu.esp = 0xfff8;
    unsigned short iret_frame[] = {0xffff, SS_SEG, 0x2, 0xf4f4};
    memcpy(_mem + _cpu.ss.base + _cpu.esp, iret_frame, sizeof(iret_frame));
    
    // our buffer resides in the ES segment
    _cpu.es.sel  = es_seg;
    _cpu.es.base = es_seg << 4;


    // start with int10 executed
    _cpu.eax      = eax;
    _cpu.edx      = edx;
    _cpu.ecx      = ecx;
    _cpu.ebx      = ebx;
    _cpu.eip      = reinterpret_cast<unsigned short *>(_mem)[0x10*2 + 0];
    _cpu.cs.sel   = reinterpret_cast<unsigned short *>(_mem)[0x10*2 + 1];
    _cpu.cs.base  = _cpu.cs.sel << 4;
    MessageExecutor msg(&_cpu, _mb.vcpustate(0));
    while (_cpu.head.pid != 12)
      {
	_instructions++;
	if (!_cpu.head.pid) _cpu.head.pid = 33;
	if (msg.vcpu->instcache->debug)
	  Logging::printf("[%x] execute at %x:%x esp %x eax %x ecx %x esi %x ebp %x\n", _instructions, _cpu.cs.sel, _cpu.eip, _cpu.esp, 
			  _cpu.eax, _cpu.ecx, _cpu.esi, _cpu.ebp);
	if (!_mb.bus_executor.send(msg, true, _cpu.head.pid))
	  Logging::panic("[%x] nobody to execute at %x:%x esp %x:%x\n", _instructions, _cpu.cs.sel, _cpu.eip, _cpu.ss.sel, _cpu.esp);
      }
    
    if ((_cpu.eax & 0xffff) == 0x004f)  return false;
    Logging::printf("VBE call(%x, %x, %x, %x) returned %x\n", eax, ecx, edx, ebx, _cpu.eax);
    return true;
  }

  template <typename T> T vbe_to_ptr(unsigned short ptr[2]) {  return reinterpret_cast<T>(_mem + ptr[0] + (ptr[1] << 4)); }

  void add_mode(unsigned short mode, unsigned seg)
  {
    Vbe::ModeInfoBlock *modeinfo = reinterpret_cast<Vbe::ModeInfoBlock *>(_mem + (seg << 4));
    
    // we only support modes with linear framebuffer
    if (modeinfo->attr & 1)
      {
	_modelist[_modecount].textmode = ~modeinfo->attr & 0x10;
	_modelist[_modecount].mode = mode | (modeinfo->attr & 0x80 ? 0xc000 : 0x8000);
	_modelist[_modecount].resolution[0] = modeinfo->resolution[0];
	_modelist[_modecount].resolution[1] = modeinfo->resolution[1];
	_modelist[_modecount].bytes_per_scanline = _vbe3 ? modeinfo->bytes_per_scanline : (modeinfo->resolution[0] * modeinfo->bpp/8);
	_modelist[_modecount].bpp = modeinfo->bpp;
	_modelist[_modecount].physbase = modeinfo->physbase;
	if (_debug)
	  Logging::printf("[%2x] %03x %s %4dx%4d-%2d phys %8x attr %x bps %x\n", _modecount, mode, _modelist[_modecount].textmode ? "window" : "linear", 
			  modeinfo->resolution[0], modeinfo->resolution[1], modeinfo->bpp, modeinfo->physbase, modeinfo->attr, _modelist[_modecount].bytes_per_scanline);
	_modecount++;
      }
  }
public:

  /**
   * Answer HostRequests from InternalHostDevices.
   */
  bool  receive(MessageHostOp &msg)
  {
    switch (msg.type)
      {
      case MessageHostOp::OP_GUEST_MEM:
	if (msg.value < _memsize)
	  {
	    msg.ptr = _mem + msg.value;
	    msg.len = _memsize - msg.value;
	    return true;
	  }
      case MessageHostOp::OP_ALLOC_IOIO_REGION:
      case MessageHostOp::OP_ALLOC_IOMEM_REGION:
	// forward to the host
	return _hostmb.bus_hostop.send(msg);
      case MessageHostOp::OP_ATTACH_HOSTIRQ:
	// we do not need this
	return false;
      case MessageHostOp::OP_UNMASK_IRQ:
      case MessageHostOp::OP_GET_MODULE:
      case MessageHostOp::OP_GET_UID:
      case MessageHostOp::OP_VIRT_TO_PHYS:
      default:
	Logging::panic("%s - unimplemented operation %x", __PRETTY_FUNCTION__, msg.type);
      }
  };


  bool  receive(MessageTimer &msg)
  {
    COUNTER_INC("requestTO");
    switch (msg.type)
      {
      case MessageTimer::TIMER_NEW:
	msg.nr = TIMER_NR;
	return true;
      case MessageTimer::TIMER_CANCEL_TIMEOUT:
	assert(msg.nr == TIMER_NR);
	return true;
      case MessageTimer::TIMER_REQUEST_TIMEOUT:
	assert(msg.nr == TIMER_NR);
	_timeout = msg.abstime;
	return true;
      default:
	return false;
      }
  }


  bool  receive(MessageIOIn   &msg) {   return _hostmb.bus_hwioin.send(msg);   }
  bool  receive(MessageIOOut  &msg) {   return _hostmb.bus_hwioout.send(msg);  }
  bool  receive(MessagePciCfg &msg) {   return _hostmb.bus_hwpcicfg.send(msg); }


  bool  receive(MessageVesa   &msg) {   

    if (msg.index < _modecount)
      switch (msg.type)
	{
	case MessageVesa::TYPE_GET_MODEINFO:
	  *msg.info = _modelist[msg.index];
	  return true;
	case MessageVesa::TYPE_SWITCH_MODE:
	  {
	    unsigned instructions = _instructions;
	    timevalue start = _hostmb.clock()->clock(1000000);
	    if (vbe_call(0x4f02, ES_SEG0, 0, 0, _modelist[msg.index].mode))  break;
	    timevalue end = _hostmb.clock()->clock(1000000);
	    Logging::printf("switch done (took %lldus, ops %d)\n", end - start, _instructions - instructions);
	    return true;
	  }
	default:
	  break;
      }
    return false;
  }


  HostVesa(Motherboard &hostmb, bool debug) : _hostmb(hostmb), _mb(hostmb.clock()), _memsize(1<<20), _timeout(~0ull), _modecount(0), _debug(debug)
  {
    _mb.bus_hostop.  add(this, &receive_static<MessageHostOp>);
    _mb.bus_timer.   add(this, &receive_static<MessageTimer>);
    _mb.bus_hwioin.  add(this, &receive_static<MessageIOIn>);
    _mb.bus_hwioout. add(this, &receive_static<MessageIOOut>);
    _mb.bus_hwpcicfg.add(this, &receive_static<MessagePciCfg>);


    MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM_REGION, 0 | 20);
    if (!_hostmb.bus_hostop.send(msg) || !msg.ptr)
      Logging::panic("%s could not map the first megabyte", __PRETTY_FUNCTION__);
    _mem = msg.ptr;

    char args[] = "mem novahalifax pit:0x40,0 scp:0x92,0x61 pcihostbridge:0,0xcf8 dpci:3,0,0,,0x21, dio:0x3c0+0x20 dio:0x3b0+0x10";
    _mb.parse_args(args);

    // check for VBE
    Vbe::InfoBlock *p = reinterpret_cast<Vbe::InfoBlock *>(_mem + (ES_SEG0 << 4));
    memcpy(p->signature, "VBE2", 4);
    if (vbe_call(0x4f00, ES_SEG0)) { Logging::printf("No VBE found\n"); return; }
    _vbe3 = p->version >= 0x300;

    Logging::printf("VBE version %x memsize %x oem '%s' vendor '%s' product '%s' version '%s'\n", 
		    p->version, 
		    p->memory << 16, 
		    vbe_to_ptr<char *>(p->oem_string),
		    vbe_to_ptr<char *>(p->oem_vendor),
		    vbe_to_ptr<char *>(p->oem_product),
		    vbe_to_ptr<char *>(p->oem_product_rev));


    // get modes
    unsigned modes = 0;
    while (modes < 32768 && vbe_to_ptr<unsigned short *>(p->video_mode)[modes] != 0xffff)
      modes++;  
    _modelist = reinterpret_cast<VesaModeInfo *>(malloc((modes + 1) * sizeof(*_modelist)));

    // add standard vga text mode
    {
      _modelist[_modecount].textmode = true;
      _modelist[_modecount].mode = 3;
      _modelist[_modecount].resolution[0] = 80;
      _modelist[_modecount].resolution[1] = 25;
      _modelist[_modecount].bpp = 4;
      _modecount++;
    }

    for (unsigned i=0; i < modes; i++)
      {
	unsigned short mode = vbe_to_ptr<unsigned short *>(p->video_mode)[i];	
	if (vbe_call(0x4f01, ES_SEG1, mode)) continue;
	add_mode(mode, ES_SEG1);
      }
    _hostmb.bus_vesa.add(this, &receive_static<MessageVesa>);
  }
};

  
PARAM(hostvesa,
      {
	HostVesa *dev = new HostVesa(mb, !argv[0]);

      },
      "hostvesa:nodebug=1 - provide a VESA console as backend for a VESA model.")
