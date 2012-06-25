/** @file
 * Host VGA console driver.
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

#include "nul/motherboard.h"
#include "service/vprintf.h"
#include "host/keyboard.h"
#include "sys/semaphore.h"

#include "nul/parent.h"
#include "nul/baseprogram.h"
#include "nul/service_timer.h"

static unsigned long _kiosk_mode = 0;

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
    TIME_TAG   = 3,       // time in seconds to display the tag
    TEXTMODE   = 0,
    BACKEND_OFFSET =   0x3000,
    BACKEND_SIZE   = 8*0x1000,
  };

private:
  Motherboard &_mb;
  char    *_backend;
  char     _saved[BACKEND_SIZE];
  char     _font [256*16];
  unsigned _modifier_switch;
  unsigned _modifier_system;
  unsigned _refresh_freq;
  unsigned _count;
  unsigned _active_client;
  unsigned _active_mode;
  unsigned short _last_cursor_pos;
  unsigned short _last_cursor_style;
  Vbe::ModeInfoBlock _modeinfo;
  KernelSemaphore  _worker;
  timevalue _lastswitchtime;
  char *_graphic_ptr;
  bool _measure;
  TimerProtocol * _timer_service;

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
    unsigned short module;
    View views[MAXVIEWS];
  } _clients[MAXCLIENTS];


  void set_vga_reg(unsigned short offset, unsigned char index, unsigned char value)
  {
    MessageHwIOOut msg1(MessageIOOut::TYPE_OUTB, 0x3c0 + offset, index);
    MessageHwIOOut msg2(MessageIOOut::TYPE_OUTB, 0x3c1 + offset, value);
    _mb.bus_hwioout.send(msg1, true) && _mb.bus_hwioout.send(msg2, true);
  };


  void update_timer(Utcb &utcb)
  {
    if (!_refresh_freq) return;

    unsigned res = _timer_service->timer(utcb, _mb.clock()->abstime(1, _refresh_freq));
    assert(!res);
  }


  bool switch_client()
  {
    _lastswitchtime =  _mb.clock()->clock(_refresh_freq);
    assert (_clients[_active_client].active_view <= _clients[_active_client].num_views);
    _measure = true;

    if (!_active_mode) set_vga_reg(0x14, 0xc, 8*3);

    // do an immediate refresh
    _worker.up();
    return true;

  }

  struct putcdata {
    char *ptr;
    unsigned pos;
    long lastchar;
  };


  /**
   * The putc on a console, removes multiple whitespaces.
   */
  static void console_putc(void *data, int value) {
    if (value < 0) return;

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

    if ((_lastswitchtime + TIME_TAG*_refresh_freq) > _mb.clock()->clock(_refresh_freq))
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
  bool handle_console_switching(MessageInput &msg)
  {
    unsigned num;

    // F1-F12 switches consoles
    if ((num = GenericKeyboard::is_function_key(msg.data, _modifier_switch))) {
      _active_client = num;
      return switch_client();
    }

    // numeric keys start new modules
    if ((num = GenericKeyboard::is_numeric_key(msg.data, _modifier_switch))) {
      if (_kiosk_mode) return false;
      MessageConsole msg1(MessageConsole::TYPE_START, num - 1);
      return _mb.bus_console.send(msg1);
    }

    // funckeys together with lctrl to debug
    if ((num = GenericKeyboard::is_function_key(msg.data, _modifier_switch ^ KBFLAG_LCTRL))) {
      if (_kiosk_mode) return false;
      MessageConsole msg1(MessageConsole::TYPE_DEBUG, num -1);
      _mb.bus_console.send(msg1);
      return false;
    }

    unsigned keycode = (msg.data & ~KBFLAG_NUM) ^ _modifier_switch;
    switch (keycode)
      {
      case 0x42: // k - kill active module
	{
	  if (_kiosk_mode) return false;
	  // switch to view 0
	  _clients[_active_client].active_view = 0;

	  // send kill message
	  MessageConsole msg1(MessageConsole::TYPE_KILL, _clients[_active_client].module);
	  _mb.bus_console.send(msg1);
	  break;
	}
      case KBCODE_RIGHT:
	_active_client = (_active_client + 1) % MAXCLIENTS;
	break;
      case KBCODE_LEFT:
	_active_client = (_active_client - 1 + MAXCLIENTS) % MAXCLIENTS;
	break;
      case KBCODE_UP:
	if (_clients[_active_client].num_views)
	  _clients[_active_client].active_view = (_clients[_active_client].active_view + 1) % _clients[_active_client].num_views;
	break;
      case KBCODE_DOWN:
	if (_clients[_active_client].num_views)
	  _clients[_active_client].active_view = (_clients[_active_client].active_view + _clients[_active_client].num_views - 1) % _clients[_active_client].num_views;
	break;
      case KBCODE_ESC: // hypervisor console
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
  bool handle_system_keys(MessageInput &msg)
  {
    unsigned keycode = (msg.data & ~KBFLAG_NUM) ^ _modifier_system;

    switch(keycode)
      {
      case KBCODE_END:
	{
	  if (_kiosk_mode) return false;
	  MessageConsole msg1(MessageConsole::TYPE_RESET);
	  return _mb.bus_console.send(msg1);
	}
      default:
	return false;
      }
  }


  bool refresh_textmode(struct View *view, Utcb &utcb)
  {
    unsigned pos = draw_console_tag();
    if (view)
      {
	if (!pos && view->direct_map) {
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
	update_timer(utcb);
      }
    else
      memset(_backend + BACKEND_OFFSET + pos, 0, 0x1000 - pos);
    return true;
  }


  void work() __attribute__((noreturn)) {
    Utcb &utcb = *BaseProgram::myutcb();
    while (1) {
      _worker.downmulti();
      unsigned mode = 0;
      struct View * view = 0;
      if (_active_client < _count && _clients[_active_client].num_views)
	{
	  assert (_clients[_active_client].active_view < _clients[_active_client].num_views);
	  COUNTER_INC("vga::refresh");

	  view = _clients[_active_client].views + _clients[_active_client].active_view;
	  mode = view->regs ? view->regs->mode : (0 + TEXTMODE);
	}
      if (mode != _active_mode)
	{
	  if (!_active_mode) memcpy(_saved, _backend, BACKEND_SIZE);

	  MessageVesa msg2(mode, &_modeinfo);
	  MessageVesa msg3(mode);
	  if (!_mb.bus_vesa.send(msg2) || !_mb.bus_vesa.send(msg3)) {
	    Logging::printf("switch vesa mode %x -> %x failed\n", _active_mode,  mode);
	    continue;
	  }

	  // get the framebuffer mapped
	  MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOMEM, _modeinfo.phys_base,  _modeinfo._phys_size);
	  if (!_mb.bus_hostop.send(msg1) || !msg1.ptr) Logging::panic("can not get the framebuffer");
	  _graphic_ptr = msg1.ptr;
	  _active_mode = mode;
	  if (!_active_mode) {
	    set_vga_reg(0x14, 0xc, 8*3);
	    memcpy(_backend, _saved, BACKEND_SIZE);
	  }
	}

      if (~_modeinfo.attr & 0x80)
	refresh_textmode(view, utcb);
      else {
	unsigned long long start = Cpu::rdtsc();
	if (_graphic_ptr)
	  memcpy(_graphic_ptr, view->ptr, _modeinfo.resolution[1]*_modeinfo.bytes_per_scanline);
	unsigned long long end = Cpu::rdtsc();
	if (_measure) Logging::printf("memcpy %d bytes took %lld cycles\n", _modeinfo.resolution[1]*_modeinfo.bytes_per_scanline/4, end - start);
	_measure = false;
	update_timer(utcb);
      }
    }
  }

public:
  bool  receive(MessageConsole &msg)
  {
    switch (msg.type)
      {
      case MessageConsole::TYPE_ALLOC_CLIENT:
       {
        // do we have a new client?
        if (_count >=  MAXCLIENTS) return false;
        unsigned client = _count++;
        _clients[client].num_views = 0;
        _clients[client].clientname = msg.clientname;
        _clients[client].module = msg.id; // Remember module for killing

        // switch in the case of a new client and if wanted (msg.view set)
        if (msg.view) {
          _active_client = client;
          switch_client();
        }
        msg.id = client;        // return console number
        return true;
       }
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
	  _worker.up();
	  return true;
	}

      case MessageConsole::TYPE_GET_MODEINFO:
	{
	  MessageVesa msg2(msg.index, msg.info);
	  return _mb.bus_vesa.send(msg2);
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
      case MessageConsole::TYPE_GET_FONT:
	memcpy(msg.ptr, _font, sizeof(_font));
	break;
      case MessageConsole::TYPE_KEY:
      case MessageConsole::TYPE_RESET:
      case MessageConsole::TYPE_START:
      case MessageConsole::TYPE_KILL:
      case MessageConsole::TYPE_DEBUG:
      default:
	break;
      }
    return false;
  }



  bool  receive(MessageInput &msg)
  {
    if (!msg.device && (handle_system_keys(msg) || handle_console_switching(msg)))
      return true;

    // default is to forward the key to the active console
    MessageConsole msg2(_active_client, _clients[_active_client].active_view, msg.device, msg.data);
    return _mb.bus_console.send(msg2);
  }


  static void do_work(void *t) REGPARM(0) NORETURN { reinterpret_cast<HostVga *>(t)->work(); }



  HostVga(Motherboard &mb, char *backend, unsigned modifier_switch, unsigned modifier_system, unsigned refresh_freq) :
    _mb(mb), _backend(backend), _saved(), _font(), _modifier_switch(modifier_switch), _modifier_system(modifier_system), _refresh_freq(refresh_freq),
    _count(0), _active_client(0), _last_cursor_pos(0), _last_cursor_style(0), _clients()
  {

    _timer_service = new TimerProtocol(alloc_cap_region(TimerProtocol::CAP_SERVER_PT + mb.hip()->cpu_desc_count(), 0));

    // get a timer
    unsigned res;
    if ((res = _timer_service->timer(*BaseProgram::myutcb(), mb.clock()->abstime(0, 1000))))
      Logging::panic("%s can't get a timer %x", __func__, res);

    // switch to sigma0 console
    set_vga_reg(0x14, 0xc, 0);

    // XXX searching would be better!
    _active_mode = TEXTMODE;
    MessageVesa msg1(_active_mode, &_modeinfo);
    if (!_mb.bus_vesa.send(msg1))
	// its a textmode, belief me
	_modeinfo.attr = 1;

    MessageHostOp msg2(MessageHostOp::OP_ALLOC_IOMEM, 0UL, 0x1000);
    if (_mb.bus_hostop.send(msg2) && msg2.ptr) {
      unsigned vec = reinterpret_cast<unsigned *>(msg2.ptr)[0x43];
      unsigned ofs = (vec & 0xffff) + ((vec >> 12) & 0xffff0);
      MessageHostOp msg3(MessageHostOp::OP_ALLOC_IOMEM, ofs & ~0xfff, sizeof(_font)*2);
      if (_mb.bus_hostop.send(msg3) && msg3.ptr) {
	//Logging::printf("got the font at %x -> %x\n", vec, ofs);
	memcpy(_font, msg3.ptr + (ofs & 0xfff), sizeof(_font));
      }
    }

    _mb.bus_input.add(this,   receive_static<MessageInput>);
    _mb.bus_console.add(this, receive_static<MessageConsole>);
    
    //Logging::printf("%s with refresh frequency %d\n", __func__, _refresh_freq);

    //use notify semaphore of timer service
    _worker = KernelSemaphore(_timer_service->get_notify_sm());

    // create the worker thread
    MessageHostOp msg5 = MessageHostOp::alloc_service_thread(do_work, this, "vga", 0);
    if (!_mb.bus_hostop.send(msg5))
      Logging::panic("%s alloc service thread failed", __func__);

  }
};

PARAM_HANDLER(kiosk_mode, "kiosk_mode - disable starting of VMs via keyboard inputs and disable debugging keys")
{ _kiosk_mode = 1; }

PARAM_HANDLER(hostvga,
	       "hostvga:<switchmodifier=LWIN><,systemmodifer=RWIN><,refresh_freq=25> - provide a VGA console.",
	       "Example: 'hostvga'.",
	       "Use 'hostvga:0x10000,0x20000' to use LCTRL and RCTRL as keymodifier."
	       "See keyboard.h for definitions.")
{
  unsigned modifier_switch = ~argv[0] ? argv[0] : (0 + KBFLAG_LWIN);
  unsigned modifier_system = ~argv[1] ? argv[1] : (0 + KBFLAG_RWIN);
  unsigned refresh_freq    = ~argv[2] ? argv[2] : 25;

  MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, 0x3c000 |  5);
  check0(!mb.bus_hostop.send(msg1), "can not allocate VGA ioports");

  MessageHostOp msg2(MessageHostOp::OP_ALLOC_IOMEM, 0xb8000, HostVga::BACKEND_SIZE);
  check0(!mb.bus_hostop.send(msg2), "can not allocate VGA backend");

  new HostVga(mb, msg2.ptr, modifier_switch, modifier_system, refresh_freq);
}
