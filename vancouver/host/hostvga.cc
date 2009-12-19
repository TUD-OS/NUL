/**
 * Host VGA console driver.
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

#include "driver/vprintf.h"
#include "models/keyboard.h"
#include "vmm/motherboard.h"

/**
 * A VGA console.
 *
 * State: unstable
 * Features: console switching, cursor, refresh, different views per client
 * Missing: direct map to the client  
 */
class HostVga : public StaticReceiver<HostVga>
{
public:
  enum { 
    MAXVIEWS   = 16,
    MAXCLIENTS = 64,
    FREQ       = 25,       // refresh FREQ in HZ
    TIME_TAG   = 3 * FREQ, // time in FREQ to display the tag
    TEXTMODE   = 0,
    BACKEND_OFFSET = 0x3000,
    BACKEND_SIZE  = 1 << 15,
  };  

private:
  Motherboard &_mb;
  char    *_backend;
  char     _saved[BACKEND_SIZE];
  unsigned _modifier_switch;
  unsigned _modifier_system;
  unsigned _count;
  unsigned _active_client;
  unsigned _active_mode;
  unsigned short _last_cursor_pos;
  unsigned short _last_cursor_style;
  VesaModeInfo _modeinfo;
  unsigned _timer;
  unsigned long long _lastswitchtime;
  char *_graphic_ptr;
  bool _measure;

  struct View {
    const char *name;
    char *   ptr;
    unsigned size;
    VgaRegs *regs;
    unsigned direct_map;
  };
  struct {
    const char *clientname;
    unsigned short num_views;
    unsigned short active_view;
    View views[MAXVIEWS];
  } _clients[MAXCLIENTS];
  
  const char *debug_getname() { return "HostVga"; };


  void set_vga_reg(unsigned short offset, unsigned char index, unsigned char value)
  {
    MessageIOOut msg1(MessageIOOut::TYPE_OUTB, 0x3c0 + offset, index);
    MessageIOOut msg2(MessageIOOut::TYPE_OUTB, 0x3c1 + offset, value);
    _mb.bus_hwioout.send(msg1) && _mb.bus_hwioout.send(msg2);
  };


  bool update_timer()
  {
    MessageTimer msg(_timer, _mb.clock()->abstime(1, FREQ));
    return _mb.bus_timer.send(msg);
  }


  bool switch_client()
  {
    _lastswitchtime =  _mb.clock()->clock(FREQ);
    assert (_clients[_active_client].active_view <= _clients[_active_client].num_views);
    _measure = true;
    if (!_active_mode) 
	set_vga_reg(0x14, 0xc, 8*3);

    // do an immediate refresh
    MessageTimeout msg(_timer);
    bool res =  receive(msg);
    return res;

  }

  struct putcdata {
    char *ptr;
    unsigned pos;
    long lastchar;
  };


  /**
   * The putc on a console, removes multiple whitespaces.
   */
  static void console_putc(void *data, long value)
    {
      putcdata *d = reinterpret_cast<putcdata *>(data);
      if (value == '\n' || value == '\t' || value == '\r' || value == 0) value = ' ';
      if (value == ' ' && d->lastchar == value) return;
      d->lastchar = value;
      d->ptr[d->pos++] = value;
      d->ptr[d->pos++] = 0x1f;
    }


  /**
   * Draw a tag for the console for a given time.
   */
  unsigned draw_console_tag()
  {
    putcdata data;
    data.pos = 0;
    data.lastchar = ' ';
    data.ptr = _backend + BACKEND_OFFSET;

    if ((_lastswitchtime + TIME_TAG) > _mb.clock()->clock(FREQ))
      {
	struct View *view = _clients[_active_client].views + _clients[_active_client].active_view;
	Vprintf::printf(console_putc, &data, "console: %d.%d", _active_client, _clients[_active_client].active_view);
	if (!_clients[_active_client].active_view && _clients[_active_client].clientname)
	  Vprintf::printf(console_putc, &data, " %s", _clients[_active_client].clientname);
	if (view->name)
	  Vprintf::printf(console_putc, &data, " '%s'", view->name);

	// fill the line to the end with whitespaces
	while ((data.pos / 2) % 80)  data.lastchar = 0, console_putc(&data,  ' ');
      }
    return data.pos;
  }

  /**
   * Switches the console with the _modifier_switch.
   */
  bool handle_console_switching(MessageKeycode &msg)
  {
    static unsigned functionkeys[] = {0x5, 0x6, 0x4, 0xc, 0x3, 0xb, 0x83, 0xa, 0x1, 0x9, 0x78, 0x7};
    static unsigned numkeys[] = {0x16, 0x1e, 0x26, 0x25, 0x2e, 0x36, 0x3d, 0x3e, 0x46, 0x45};

    unsigned keycode = (msg.keycode & ~KBFLAG_NUM) ^ _modifier_switch;

    // F1-F12 switches consoles
    for (unsigned i=0;  i < sizeof(functionkeys)/sizeof(*functionkeys); i++)
      if (keycode == functionkeys[i]) 
	{
	  _active_client = i + 1;
	  return switch_client();
	}
    
    // numeric keys start new modules
    for (unsigned i=0;  i < sizeof(numkeys)/sizeof(*numkeys); i++)
      if (keycode == numkeys[i]) 
	{
	  MessageConsole msg1(MessageConsole::TYPE_START, i);
	  return _mb.bus_console.send(msg1);
	}
    
    // funckeys together with lctrl to debug
      else if (keycode == (functionkeys[i] | KBFLAG_LCTRL))
	{
	  MessageConsole msg1(MessageConsole::TYPE_DEBUG, i);
	  _mb.bus_console.send(msg1);
	  return false;
	}

    switch (keycode)
      {
      case KBFLAG_EXTEND0 | 0x74:  // right
	_active_client = (_active_client + 1) % MAXCLIENTS;
	break;
      case KBFLAG_EXTEND0 | 0x6b:  // left
	_active_client = (_active_client - 1 + MAXCLIENTS) % MAXCLIENTS;
	break;
      case KBFLAG_EXTEND0 | 0x75:  // up
	if (_clients[_active_client].num_views)
	  _clients[_active_client].active_view = (_clients[_active_client].active_view + 1) % _clients[_active_client].num_views;
	break;
      case KBFLAG_EXTEND0 | 0x72:  // down
	if (_clients[_active_client].num_views)
	  _clients[_active_client].active_view = (_clients[_active_client].active_view + _clients[_active_client].num_views - 1) % _clients[_active_client].num_views;
	break;
      case 0x76: // ESC -> hypervisor console
	_active_client = 0;
	break;	
      default:
	return false;
      }    
    return switch_client();
  }


  /**
   * System keys handling.
   */
  bool handle_system_keys(MessageKeycode &msg)
  {
    unsigned keycode = (msg.keycode & ~KBFLAG_NUM) ^ _modifier_system;
      
    switch(keycode)
      {
      case KBFLAG_EXTEND0 | 0x69: // end
	{
	  MessageConsole msg1(MessageConsole::TYPE_RESET);
	  return _mb.bus_console.send(msg1);
	}
      default:
	return false;
      }
    return false;
  }


  bool refresh_textmode(struct View *view)
  {
    unsigned pos = draw_console_tag();
    if (view)
      {
	if (/*!pos &&*/ view->direct_map) {
	  set_vga_reg(0x14, 0xc, (view->direct_map - 0xb8000) >> 9);
	  return true;
	}
	COUNTER_INC("vga::copy");
    
	unsigned short cursor_offset = view->regs->cursor_pos -  view->regs->offset;
	if (cursor_offset != _last_cursor_pos)
	  {
	    _last_cursor_pos = cursor_offset;
	    set_vga_reg(0x14, 0xe, 3*8 + (cursor_offset >> 8));
	    set_vga_reg(0x14, 0xf, cursor_offset);
	  }
	if (view->regs->cursor_style !=  _last_cursor_style)
	  {
	    _last_cursor_style = view->regs->cursor_style;
	    set_vga_reg(0x14, 0xa, view->regs->cursor_style >> 8);
	    set_vga_reg(0x14, 0xb, view->regs->cursor_style);
	  }
    
	unsigned len = 0x1000 - pos;
	unsigned offset = view->regs->offset << 1;
	if (offset < view->size)
	  {
	    unsigned sublen = 0;
	    if (len + offset > view->size) {
	      sublen = len;
	      len = view->size - offset;
	      sublen -= len;
	    }
	    memcpy(_backend + BACKEND_OFFSET + pos, reinterpret_cast<char *>(view->ptr) + pos + offset, len);
	    memcpy(_backend + BACKEND_OFFSET + pos + len, reinterpret_cast<char *>(view->ptr), sublen);
	  }
	else
	  memset(_backend + BACKEND_OFFSET + pos, 0, 0x1000 - pos);
	update_timer();
      }
    else
      memset(_backend + BACKEND_OFFSET + pos, 0, 0x1000 - pos);
    return true;
  }

  static
  void
  memcpyl(void *dst, const void *src, long count)
  {
    asm volatile ("rep movsl" : "+D"(dst), "+S"(src), "+c"(count) :  : "memory");
  }



public:
  bool  receive(MessageTimeout &msg)
  {
    if (msg.nr == _timer)
      {
	unsigned mode = 0;
	struct View * view = 0;
	if (_active_client < _count && _clients[_active_client].num_views)
	  {
	    assert (_clients[_active_client].active_view < _clients[_active_client].num_views);
	    COUNTER_INC("vga::refresh");

	    view = _clients[_active_client].views + _clients[_active_client].active_view;
	    mode = view->regs ? view->regs->mode : (0+TEXTMODE);
	  }
	if (mode != _active_mode)
	  {
	    if (!_active_mode) memcpy(_saved, _backend, BACKEND_SIZE);

	    MessageVesa msg2(mode, &_modeinfo);
	    MessageVesa msg3(mode);
	    if (!_mb.bus_vesa.send(msg2) || !_mb.bus_vesa.send(msg3)) 
	      {
		Logging::printf("switch vesa mode %x -> %x failed\n", _active_mode,  mode);
		return false;
	      }

	    // get the framebuffer mapped
	    MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOMEM, _modeinfo.physbase, 1<<22);
	    if (!_mb.bus_hostop.send(msg1)) Logging::panic("can not get the framebuffer");
	    _graphic_ptr = msg1.ptr;
	    _active_mode = mode;
	    if (!_active_mode) memcpy(_backend, _saved, BACKEND_SIZE);
	  }

	if (_modeinfo.textmode)
	  return refresh_textmode(view);
	else
	  {
	    unsigned long long start = Cpu::rdtsc();
	    memcpyl(_graphic_ptr, view->ptr, _modeinfo.resolution[1]*_modeinfo.bytes_per_scanline/4);
	    unsigned long long end = Cpu::rdtsc();
	    if (_measure) Logging::printf("memcpy %d bytes took %lld cycles\n", _modeinfo.resolution[1]*_modeinfo.bytes_per_scanline/4, end - start);
	    _measure = false;
	    update_timer();
	    return true;
	  }
	
	return false;
      }
    return false;
  }


  bool  receive(MessageConsole &msg)
  {
    switch (msg.type)
      {
      case MessageConsole::TYPE_ALLOC_CLIENT:
	// do we have a new client?
	if (_count >=  MAXCLIENTS) return false;
	msg.id = _count++;
	_clients[msg.id].num_views = 0;
	_clients[msg.id].clientname = msg.clientname;

	// switch in the case of a new client
	_active_client = msg.id;
	switch_client();
	return true;
      case MessageConsole::TYPE_ALLOC_VIEW:
	{
	  if (msg.id >=  _count) return false;
	  if (_clients[msg.id].num_views >=  MAXVIEWS) return false;
	  assert(msg.ptr && msg.regs);
	  msg.view = _clients[msg.id].num_views++;
	  struct View *view = _clients[msg.id].views + msg.view;
	  view->name = msg.name;
	  view->ptr = msg.ptr;
	  view->size = msg.size;
	  view->regs = msg.regs;
	  MessageHostOp msg1(MessageHostOp::OP_VIRT_TO_PHYS, reinterpret_cast<unsigned long>(msg.ptr));
	  if (_mb.bus_hostop.send(msg1) && msg1.phys >= 0xb8000 && msg1.phys < 0xc0000 && 0x1000 <= msg1.phys_len)
	    view->direct_map = msg1.value;
	  else
	    view->direct_map = 0;
	  
	  // do an immediate refresh
	  MessageTimeout msg2(_timer);
	  receive(msg2);
	  //Logging::printf("allocated view %x.%x ptr %p\n", msg.id, msg.view, msg.ptr);
	  return true;
	}

      case MessageConsole::TYPE_GET_MODEINFO:
	{
	  VesaModeInfo info;
	  MessageVesa msg2(msg.index, &info);
	  if (!_mb.bus_vesa.send(msg2)) return false;
	  msg.info->textmode = info.textmode;
	  msg.info->vesamode = info.mode;
	  msg.info->bpp = info.bpp;
	  msg.info->resolution[0] = info.resolution[0];
	  msg.info->resolution[1] = info.resolution[1];
	  msg.info->bytes_per_scanline = info.bytes_per_scanline;
	  return true;
	};
      // switch to another view
      case MessageConsole::TYPE_SWITCH_VIEW:
	if ((msg.id >= _count) || (msg.view >= _clients[msg.id].num_views)) return false;
	_clients[msg.id].active_view = msg.view;

	// allow the first client to also switch to its window
	if (!msg.id)
	  {
	    _active_client = msg.id;
	    switch_client();
	  }
	break;
      case MessageConsole::TYPE_KEY:
      case MessageConsole::TYPE_RESET:
      case MessageConsole::TYPE_START:
      case MessageConsole::TYPE_DEBUG:
      default: 
	break;
      }
    return false;
  }
  



  bool  receive(MessageKeycode &msg)
  {    
    if (msg.keyboard != 0) return false;
    
    if (handle_system_keys(msg) || handle_console_switching(msg)) return true;
	
    // default is to forward the key to the active console
    MessageConsole msg2(_active_client, _clients[_active_client].active_view, msg.keycode);
    return _mb.bus_console.send(msg2);
  }


  HostVga(Motherboard &mb, char *backend, unsigned modifier_switch, unsigned modifier_system) : 
    _mb(mb), _backend(backend), _modifier_switch(modifier_switch), _modifier_system(modifier_system), 
    _count(0), _active_client(0), _last_cursor_pos(0), _last_cursor_style(0)
  {
    memset(_clients, 0, sizeof(_clients));
    _mb.bus_keycode.add(this, &HostVga::receive_static<MessageKeycode>);
    _mb.bus_console.add(this, &HostVga::receive_static<MessageConsole>);
    _mb.bus_timeout.add(this, &HostVga::receive_static<MessageTimeout>);

    MessageTimer msg0;
    if (!_mb.bus_timer.send(msg0))
      Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);
    _timer = msg0.nr;

    // switch to sigma0 console
    set_vga_reg(0x14, 0xc, 0);

    // XXX searching would be better!
    _active_mode = 0;
    _modeinfo.textmode = true;

    Logging::printf("%s with refresh FREQ %d\n", __func__, FREQ);
    
  }
};

  
PARAM(hostvga,
      {
	unsigned modifier_switch = KBFLAG_LWIN;
	unsigned modifier_system = KBFLAG_RWIN;
	if (~argv[0]) modifier_switch = argv[0];
	if (~argv[1]) modifier_system = argv[1];

	MessageHostOp msg(MessageHostOp::OP_ALLOC_IOMEM, 0xb8000, HostVga::BACKEND_SIZE);
	if (!mb.bus_hostop.send(msg)) Logging::panic("can not allocate VGA backend");	
	HostVga *dev = new HostVga(mb, msg.ptr, modifier_switch, modifier_system);
      },
      "hostvga:<switchmodifier=LWIN><,systemmodifer=RWIN> - provide a VGA console.",
      "Example: 'hostvga'.",
      "Use 'hostvga:0x10000,0x20000' to use LCTRL and RCTRL as keymodifier."
      "See keyboard.h for definitions.")
