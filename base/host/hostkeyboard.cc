/** @file
 * HostKeyboard driver.
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
#include "host/keyboard.h"

#define LOG if (_verbose) Logging::printf

/**
 * A PS/2 host keyboard and mouse driver.  Translates SCS2 keycodes to
 * single extended keycode and mouse movements to mouse packets.  Both
 * are forwarded on the keycode bus.
 *
 * State: stable
 * Features: scancode set1+2, simple PS2 mouse
 * Missing: z-axis
 * Documentation: PS2 hitrc chapter 7+11, scancodes-13.html
 */
class HostKeyboard : public StaticReceiver<HostKeyboard>
{
  #include "host/simplehwioin.h"
  #include "host/simplehwioout.h"

  DBus<MessageInput> & _bus_input;
  Clock *_clock;
  unsigned _hostdev;
  unsigned short _base;
  unsigned _irq;
  unsigned _irqaux;
  unsigned _flags;
  unsigned _mousestate;
  bool _scset1;
  bool _verbose;
  static unsigned const FREQ = 1000;
  static unsigned const TIMEOUT = 50;


  bool wait_status(unsigned char mask, unsigned char value)
  {
    timevalue timeout = _clock->abstime(TIMEOUT, FREQ);
    unsigned char status;
    do
      status = inb(_base + 4);
    while ((status & mask) != value && _clock->time() < timeout);
    return (status & mask) == value;
  }


  bool wait_output_full()  {  return wait_status(0x1, 1); }
  bool wait_input_empty()  {  return wait_status(0x2, 0); }
  bool disable_devices()
  {
    if (!wait_input_empty()) return false;
    outb(0xad, _base + 4);
    if (!wait_input_empty()) return false;
    outb(0xa7, _base + 4);
    // drop output buffer?
    while (inb(_base + 4) & 1) inb(_base);

    if (!wait_input_empty()) return false;
    assert(!(inb(_base + 4) & 1));
    return true;
  }

  bool enable_devices()
  {
    if (!wait_input_empty()) return false;
    outb(0xae, _base + 4);
    if (!wait_input_empty()) return false;
    outb(0xa8, _base + 4);
    if (!wait_input_empty()) return false;
    return true;
  }

  bool read_cmd(unsigned char cmd, unsigned char &value)
  {
    if (!wait_input_empty()) return false;
    outb(cmd, _base + 4);
    if (!wait_output_full()) return false;
    value = inb(_base);
    return true;
  }


  bool write_cmd(unsigned char cmd, unsigned char value)
  {
    if (!wait_input_empty()) return false;
    outb(cmd, _base + 4);
    if (!wait_input_empty()) return false;
    outb(value,_base);
    return true;
  }

  bool wait_ack()
  {
    unsigned char status;
    timevalue timeout = _clock->abstime(TIMEOUT, FREQ);
    do
      {
	status = inb(_base + 4);
	if (status & 1 && inb(_base) == 0xfa && ~status & 0x20)
	  return true;
      }
    while (_clock->time() < timeout);
    return false;
  }

  bool write_keyboard_ack(unsigned char value)
  {
    if (!wait_input_empty()) return false;
    outb(value,_base);
    return wait_ack();
  }


  bool write_mouse_ack(unsigned char value)
  {
    if (!wait_input_empty()) return false;
    write_cmd(0xd4, value);
    return wait_ack();
  }


  void handle_aux(unsigned char data)
  {
    switch (_mousestate & 0xff)
      {
      case 0xfe: // wait for reset ack
	if (data == 0xaa)
	  _mousestate++;
	else
	  LOG("kb: %s no reset ack %x\n", __PRETTY_FUNCTION__, data);
	return;
      case 0xff: // wait for reset id
	if (data == 0)
	  {
	    _mousestate = 0;
	    if (!write_mouse_ack(0xf4))
	      LOG("kb: %s could not enable data reporting\n", __PRETTY_FUNCTION__);
	  }
	else
	  Logging::printf("kb: %s unknown mouse id %x\n", __PRETTY_FUNCTION__, data);
	return;
      default:
      case 0:  // first byte of packet
	if (~data & 0x8) // not in sync?
	  {
	    LOG("kb: %s mouse not in sync - drop %x\n", __PRETTY_FUNCTION__, data);
	    return;
	  }
      case 1 ... 2:
	_mousestate++;
	_mousestate |= data << (8 * (_mousestate & 0xff));
	if ((_mousestate & 0xff) != 3)
	  return;
      }
    MessageInput msg(_hostdev+1, _mousestate);
    _bus_input.send(msg);
    _mousestate = 0;
  }


  /**
   * Handle a scancode send from the keyboard.
   */
  void handle_scancode(unsigned char key)
  {

    /**
     * There are some bad BIOSes around which do not emulate SC2.
     * We have to convert from SC1.
     */
    if (_scset1)
      {
	if (key & 0x80 && key != 0xe0 && key != 0xe1)  _flags |= KBFLAG_RELEASE;
	key = GenericKeyboard::translate_sc1_to_sc2(key);
      }

    /**
     * We have a small state machine here, as the keyboard runs with
     * scancode set 2. SCS3 would be much nicer but is not available
     * everywhere.
     */
    unsigned nflag = 0;
    switch (key)
      {
      case 0x00:        // overrun
	_flags &= KBFLAG_NUM;
	return;
      case 0xE0:
	_flags |= KBFLAG_EXTEND0;
	return;
      case 0xE1:
	_flags |= KBFLAG_EXTEND1;
	return;
      case 0xF0:
	_flags |= KBFLAG_RELEASE;
	return;
      case 0x11:	// alt
	nflag = _flags & KBFLAG_EXTEND0 ? KBFLAG_RALT : KBFLAG_LALT;
	break;
      case 0x12:	// lshift
      case 0x59:	// rshift
	if (_flags & KBFLAG_EXTEND0)
	  {
	    // ignore extended modifiers here
	    _flags &= ~(KBFLAG_EXTEND0 | KBFLAG_RELEASE);
	    return;
	  }
	nflag = key == 0x12  ? KBFLAG_LSHIFT : KBFLAG_RSHIFT;
	break;
      case 0x14:	// ctrl
	nflag = _flags & KBFLAG_EXTEND0 ? KBFLAG_RCTRL : KBFLAG_LCTRL;
	break;
      case 0x1f:
	nflag = _flags & KBFLAG_EXTEND0 ? KBFLAG_LWIN : 0;
	break;
      case 0x27:
	nflag = _flags & KBFLAG_EXTEND0 ? KBFLAG_RWIN : 0;
	break;
      case KBCODE_NUM:
	if (!(_flags & (KBFLAG_EXTEND1 | KBFLAG_RELEASE)))
	  {
	    _flags ^= KBFLAG_NUM;
	    write_keyboard_ack(0xed) && write_keyboard_ack(_flags & KBFLAG_NUM ? 0x2 : 0);
	  }
      default:
	;
      }

    // the-break-key - drop the break keycodes
    if (_flags & KBFLAG_EXTEND0 && ~_flags & KBFLAG_RELEASE && key == 0x7e)
      return;

    // the normal pause key - drop the break keycodes
    if (_flags & KBFLAG_EXTEND1 && ((key == 0x14) || (key==0x77 && ~_flags & KBFLAG_RELEASE)))
      return;

    if (_flags & KBFLAG_RELEASE)
      _flags &= ~nflag;
    else
      _flags |=  nflag;

    MessageInput msg(_hostdev, _flags | key);
    _bus_input.send(msg);
    _flags &= ~(KBFLAG_RELEASE | KBFLAG_EXTEND0 | KBFLAG_EXTEND1);
  }

 public:
  bool  receive(MessageIrq &msg)
  {
    if ((msg.line == _irq ||  msg.line == _irqaux) && msg.type == MessageIrq::ASSERT_IRQ)
      {
	unsigned char status;
	while ((status = inb(_base + 4)) & 1)
	  {
	    unsigned char data = inb(_base);
	    if (status & 0x20)
	      handle_aux(data);
	    else
	      handle_scancode(data);
	  }
	return true;
      }
    return false;
  }


  bool  receive(MessageLegacy &msg)
  {
    if (msg.type == MessageLegacy::RESET)
      {
	unsigned irq = _irq;
	unsigned irqaux = _irqaux;
	_irq = ~0u;
	_irqaux = ~0u;

	unsigned char  cmdbyte = 0;

	#if 0
	if (!disable_devices())
	  LOG("kb: %s() failed at %d with %x\n",__func__, __LINE__, inb(_base+4));
	#endif

	// clear keyboard buffer
	while (inb(_base + 4) & 1) inb(_base);


	if (!read_cmd(0x20, cmdbyte))
	  LOG("kb: %s() failed at %d\n",__func__, __LINE__);

	cmdbyte &= ~0x40;
	// we enable translation if scset == 1
	if (_scset1)  cmdbyte |= 0x40;

	// set translation and enable irqs
	if (!write_cmd(0x60, cmdbyte | 3) || !read_cmd(0x20, cmdbyte))
	  LOG("kb: %s() failed at %d\n",__func__, __LINE__);
	_scset1 |= !!(cmdbyte & 0x40);

	if (!enable_devices())
	  LOG("kb: %s() failed at %d\n",__func__, __LINE__);

	// default+disable Keyboard
	if (!write_keyboard_ack(0xf5))
	  LOG("kb: %s() failed at %d\n",__func__, __LINE__);

	// switch to our scancode set
	if (!(write_keyboard_ack(0xf0) && write_keyboard_ack(2)))
	  {
	    Logging::printf("kb: %s() failed at %d -- buggy keyboard?\n",__func__, __LINE__);
	    _scset1 = true;
	  }

	// enable Keyboard
	if (!write_keyboard_ack(0xf4))
	  LOG("kb: %s() failed at %d\n",__PRETTY_FUNCTION__, __LINE__);

	if (irqaux != ~0U)
	  {
	    // reset mouse, we enable data reporting later after the reset is completed
	    if (!write_mouse_ack(0xff))
	      LOG("kb: %s() failed at %d\n",__func__, __LINE__);

	    // wait until we got response from the mice
	    if (!wait_output_full())
	      LOG("kb: %s() failed at %d\n",__func__, __LINE__);
	  }

	// enable receiving
	_irq = irq;
	_irqaux = irqaux;
	_flags = KBFLAG_NUM;
	_mousestate = 0xfe;

	// consume pending characters
	MessageIrq msg1(MessageIrq::ASSERT_IRQ, _irq);
	receive(msg1);
      }
    return false;
  }


  HostKeyboard(DBus<MessageHwIOIn> &bus_hwioin, DBus<MessageHwIOOut> &bus_hwioout, DBus<MessageInput> &bus_input,
	       Clock *clock, unsigned hostdev, unsigned short base,
	       unsigned irq, unsigned irqaux, unsigned char scset, bool verbose)
    : _bus_hwioin(bus_hwioin), _bus_hwioout(bus_hwioout), _bus_input(bus_input),
      _clock(clock), _hostdev(hostdev), _base(base),
      _irq(irq), _irqaux(irqaux), _scset1(scset == 1), _verbose(verbose)
    {}

};

static bool default_mouse_enabled = false;

PARAM_HANDLER(enable_ps2_mouse) { default_mouse_enabled = true; }

PARAM_HANDLER(hostkeyb,
	      "hostkeyb:hdev=0,hostiobase=0x60,kbirq=1,auxirq=0,scset=2,verbose=0 - provide an input backend from the host keyboard (hdev) and host mouse (hdev+1).",
	      "Example: 'hostkeyb:0x17,0x60,1,12,2'.",
	      "A missing auxirq omits the mouse initialisation. ")
{
  unsigned hdev       = ~argv[0] ? argv[0] : 0;
  uint16   hostiobase = ~argv[1] ? argv[1] : 0x60;
  unsigned kbirq      = ~argv[2] ? argv[2] : 1;
  unsigned auxirq     = ~argv[3] ? argv[3] : (default_mouse_enabled ? 12 : ~0U);

  MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, (hostiobase << 8) |  0);
  MessageHostOp msg2(MessageHostOp::OP_ALLOC_IOIO_REGION, ((hostiobase + 4) << 8) |  0);
  if (!mb.bus_hostop.send(msg1) || !mb.bus_hostop.send(msg2))
    Logging::panic("%s failed to allocate ports %x, %x\n", __PRETTY_FUNCTION__, hostiobase, hostiobase+4);

  HostKeyboard *dev = new HostKeyboard(mb.bus_hwioin, mb.bus_hwioout, mb.bus_input, mb.clock(), hdev, hostiobase, kbirq, auxirq, argv[4], ~argv[5] ? argv[5] : 0);
  mb.bus_hostirq.add(dev, HostKeyboard::receive_static<MessageIrq>);
  mb.bus_legacy.add(dev,  HostKeyboard::receive_static<MessageLegacy>);

  MessageHostOp msg3 = MessageHostOp::attach_irq(kbirq, ~0U, true, "keyb");
  MessageHostOp msg4 = MessageHostOp::attach_irq(auxirq, ~0U, true, "aux");
  if (!(msg3.value == ~0U || mb.bus_hostop.send(msg3)) || !(msg4.value == ~0U || mb.bus_hostop.send(msg4)))
    Logging::panic("%s failed to attach hostirq %x, %x\n", __PRETTY_FUNCTION__, kbirq, auxirq);
}
