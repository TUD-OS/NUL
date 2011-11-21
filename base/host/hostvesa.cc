/** @file
 * Host VESA console driver.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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

#include "host/vesa.h"
#include "nul/motherboard.h"
#include "nul/vcpu.h"

/**
 * A Vesa console.
 *
 * State: unstable
 * Features: get linear modes, validate physpointer, set LinBytesPerScanline, timeouts
 * Missing:  vesa1.2 support
 */
class HostVesa : public StaticReceiver<HostVesa>
{
  enum {
    SS_SEG   = 0x1000,
    ES_SEG0  = 0x2000,
    ES_SEG1  = 0x3000,
    TIMER_NR = 2
  };

  Motherboard  & _hostmb;
  Motherboard    _mb;
  char         * _mem;
  unsigned long  _memsize;
  CpuState       _cpu;
  timevalue      _timeout;

  unsigned long  _framebuffer_size;
  unsigned long  _framebuffer_phys;

  unsigned short       _version;
  unsigned short       _modecount;
  Vbe::ModeInfoBlock * _modelist;
  unsigned             _instructions;
  unsigned             _debug;


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
    _cpu.set_header(EXCEPTION_WORDS, 0);
    _cpu.mtd      = MTD_ALL;
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

    CpuMessage msg0(CpuMessage::TYPE_SINGLE_STEP, &_cpu, MTD_ALL);
    CpuMessage msg1(CpuMessage::TYPE_CHECK_IRQ, &_cpu, MTD_ALL);
    while (!_cpu.actv_state)
      {
	_instructions++;
	if (_debug & 2)
	  Logging::printf("[%x] execute at %x:%x esp %x eax %x ecx %x esi %x ebp %x efl %x\n", _instructions, _cpu.cs.sel, _cpu.eip, _cpu.esp,
			  _cpu.eax, _cpu.ecx, _cpu.esi, _cpu.ebp, _cpu.efl);
	if (!_mb.last_vcpu->executor.send(msg0))
	  Logging::panic("[%x] nobody to execute at %x:%x esp %x:%x\n", _instructions, _cpu.cs.sel, _cpu.eip, _cpu.ss.sel, _cpu.esp);
	if (!_mb.last_vcpu->executor.send(msg1))
	  Logging::panic("[%x] nobody to execute at %x:%x esp %x:%x\n", _instructions, _cpu.cs.sel, _cpu.eip, _cpu.ss.sel, _cpu.esp);

	if (_mb.clock()->time() > _timeout) {
	  MessageTimeout msg2(TIMER_NR, _timeout);
	  _timeout = ~0ull;
	  _mb.bus_timeout.send(msg2);
	}
      }

    if ((_cpu.eax & 0xffff) == 0x004f)  return false;
    Logging::printf("VBE call(%x, %x, %x, %x) returned %x\n", eax, ecx, edx, ebx, _cpu.eax);
    return true;
  }

  template <typename T> T vbe_to_ptr(unsigned ptr) {  return reinterpret_cast<T>(_mem + (ptr & 0xffff) + ((ptr >> 12) & 0xffff0)); }

  void add_mode(unsigned short mode, unsigned seg, unsigned min_attributes)
  {
    Vbe::ModeInfoBlock *modeinfo = reinterpret_cast<Vbe::ModeInfoBlock *>(_mem + (seg << 4));
    if ((modeinfo->attr & min_attributes) == min_attributes)
      {
	// keeep vesa modenumber for further reference
	modeinfo->_vesa_mode = mode;
	if (_version < 0x200 || !in_range(modeinfo->phys_base, _framebuffer_phys, _framebuffer_size)) modeinfo->phys_base = _framebuffer_phys;
	modeinfo->_phys_size = modeinfo->phys_base - _framebuffer_phys + _framebuffer_size;

	// validate bytes_per_scanline
	if (_version < 0x300 || !modeinfo->bytes_per_scanline) modeinfo->bytes_per_scanline = (modeinfo->resolution[0] * modeinfo->bpp / 8);

	memcpy(_modelist + _modecount, modeinfo, sizeof(*_modelist));
	if (_debug)
	  Logging::printf("[%2x] %03x %s %4dx%4d-%2d phys %8x+%8x attr %x bps %x planes %d mmdl %d\n", _modecount, mode, (modeinfo->attr & 0x80) ? "linear" : "window",
			  modeinfo->resolution[0], modeinfo->resolution[1], modeinfo->bpp, modeinfo->phys_base, modeinfo->_phys_size,
			  modeinfo->attr, modeinfo->bytes_per_scanline, modeinfo->planes, modeinfo->memory_model);
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
      case MessageHostOp::OP_ALLOC_IOMEM:
	if (msg.len > _framebuffer_size)
	  {
	    _framebuffer_size = msg.len;
	    _framebuffer_phys = msg.value;
	  }
      case MessageHostOp::OP_ALLOC_IOIO_REGION:
      case MessageHostOp::OP_ASSIGN_PCI:
      case MessageHostOp::OP_ATTACH_IRQ:
      case MessageHostOp::OP_ATTACH_MSI:
	// forward to the host
	return _hostmb.bus_hostop.send(msg);
      case MessageHostOp::OP_VCPU_BLOCK:
	// invalid value, to abort the loop
	_cpu.actv_state = 0x80000000;
      case MessageHostOp::OP_VCPU_CREATE_BACKEND:
	return true;
      case MessageHostOp::OP_VCPU_RELEASE:
      case MessageHostOp::OP_NOTIFY_IRQ:
      case MessageHostOp::OP_GET_MODULE:
      case MessageHostOp::OP_GET_MAC:
      case MessageHostOp::OP_VIRT_TO_PHYS:
      case MessageHostOp::OP_ALLOC_FROM_GUEST:
      case MessageHostOp::OP_ALLOC_SEMAPHORE:
      case MessageHostOp::OP_ALLOC_SERVICE_THREAD:
      case MessageHostOp::OP_REGISTER_SERVICE:
      case MessageHostOp::OP_WAIT_CHILD:
      case MessageHostOp::OP_ALLOC_SERVICE_PORTAL:
      case MessageHostOp::OP_CREATE_EC4PT:
      default:
	Logging::panic("%s - unimplemented operation %x", __PRETTY_FUNCTION__, msg.type);
      }
  };


  bool  receive(MessageTimer &msg)
  {
    switch (msg.type)
      {
      case MessageTimer::TIMER_NEW:
	msg.nr = TIMER_NR;
	return true;
      case MessageTimer::TIMER_REQUEST_TIMEOUT:
	assert(msg.nr == TIMER_NR);
	_timeout = msg.abstime;
	return true;
      default:
	return false;
      }
  }


  bool  receive(MessageIOIn      &msg) {  return _hostmb.bus_hwioin.send(static_cast<MessageHwIOIn&>(msg), true);   }
  bool  receive(MessageIOOut     &msg) {  return _hostmb.bus_hwioout.send(static_cast<MessageHwIOOut&>(msg), true);  }
  bool  receive(MessageHwPciConfig &msg) {  return _hostmb.bus_hwpcicfg.send(msg, true); }
  bool  receive(MessageVesa   &msg)
  {
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
	    unsigned mode = _modelist[msg.index]._vesa_mode;
	    if (_modelist[msg.index].attr & 0x80) mode |= 0xc000;
	    if (vbe_call(0x4f02, ES_SEG0, 0, 0, mode))  break;
	    timevalue end = _hostmb.clock()->clock(1000000);
	    Logging::printf("switch to %x done (took %lldus, ops %d)\n", mode, end - start, _instructions - instructions);
	    return true;
	  }
	default:
	  break;
	}
    return false;
  }


  HostVesa(Motherboard &hostmb, unsigned debug) : _hostmb(hostmb), _mb(hostmb.clock(), hostmb.hip()), _memsize(1<<20), _timeout(~0ull),
						  _framebuffer_size(0), _framebuffer_phys(0),
						  _modecount(0), _debug(debug)
  {
    _mb.bus_hostop.  add(this, receive_static<MessageHostOp>);
    _mb.bus_timer.   add(this, receive_static<MessageTimer>);
    _mb.bus_hwioin.  add(this, receive_static<MessageHwIOIn>);
    _mb.bus_hwioout. add(this, receive_static<MessageHwIOOut>);
    _mb.bus_hwpcicfg.add(this, receive_static<MessageHwPciConfig>);


    MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, 0UL , 1<<20);
    if (!_hostmb.bus_hostop.send(msg) || !msg.ptr)
      Logging::panic("%s could not map the first megabyte", __PRETTY_FUNCTION__);
    _mem = msg.ptr;
    _mb.parse_args("mem pit:0x40,0 scp:0x92,0x61 pcihostbridge:0,0x100,0xcf8 dpci:3,0,0,0,0,0 dio:0x3c0+0x20 dio:0x3b0+0x10 vcpu halifax");


    // initialize PIT0

    uint16 pic_init[][2] = { { 0x43, 0x24 }, // let counter0 count with minimal freq of 18.2hz
                             { 0x40, 0x00 },
                             { 0x43, 0x56 }, // let counter1 generate 15usec refresh cycles
                             { 0x41, 0x12 } };

    for (unsigned i = 0; i < sizeof(pic_init)/sizeof(pic_init[0]); i++) {
      MessageIOOut m(MessageIOOut::TYPE_OUTB, pic_init[i][0], pic_init[i][1]);
      _mb.bus_ioout.send(m);
    }

    // check for VBE
    Vbe::InfoBlock *p = reinterpret_cast<Vbe::InfoBlock *>(_mem + (ES_SEG0 << 4));
    p->tag = Vbe::TAG_VBE2;
    if (vbe_call(0x4f00, ES_SEG0)) { Logging::printf("No VBE found\n"); return; }
    if (p->version < 0x200)  { Logging::printf("VBE version %x too old\n", p->version); return; }

    // we need only the version from the InfoBlock
    _version = p->version;
    Logging::printf("VBE version %x tag %x memsize %x oem '%s' vendor '%s' product '%s' version '%s'\n",
		    p->version,
		    p->tag,
		    p->memory << 16,
		    vbe_to_ptr<char *>(p->oem_string),
		    vbe_to_ptr<char *>(p->oem_vendor),
		    vbe_to_ptr<char *>(p->oem_product),
		    vbe_to_ptr<char *>(p->oem_product_rev));


    // get modes
    unsigned modes = 0;
    unsigned short *video_mode_ptr  = vbe_to_ptr<unsigned short *>(p->video_mode_ptr);
    while (modes < 32768 && video_mode_ptr[modes] != 0xffff)
      modes++;
    _modelist = new Vbe::ModeInfoBlock[modes + 1];


    // add standard vga text mode #3
    {
      _modelist[_modecount]._vesa_mode = 3;
      _modelist[_modecount].attr = 0x1;
      _modelist[_modecount].resolution[0] = 80;
      _modelist[_modecount].resolution[1] = 25;
      _modelist[_modecount].bytes_per_scanline = 80*2;
      _modelist[_modecount].bpp = 4;
      _modelist[_modecount].phys_base = 0xb8000;
      _modelist[_modecount]._phys_size = 0x8000;
      _modecount++;
    }

    // add modes with linear framebuffer
    for (unsigned i=0; i < modes; i++)
      {
	unsigned short mode = vbe_to_ptr<unsigned short *>(p->video_mode_ptr)[i];
	if (!vbe_call(0x4f01, ES_SEG1, mode))  add_mode(mode, ES_SEG1, 0x81);
      }
    _hostmb.bus_vesa.add(this, receive_static<MessageVesa>);
    Logging::printf("framebuffer %lx+%lx\n", _framebuffer_phys, _framebuffer_size);
  }
};


PARAM_HANDLER(hostvesa,
	      "hostvesa:debug=0 - provide a VESA console as backend for a VESA model.",
	      "Use 'hostvesa:3' for debug output.")
{
  new HostVesa(mb, ~argv[0] ? argv[0] : 0);
}
