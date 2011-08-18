/** @file
 * PIC8259 emulation.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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

/**
 * An implementation of the Intel 8259.
 *
 * State: stable
 * Ignored bits: ADI, yPM
 * Documentation: Intel 8259a - 8259A.pdf
 */
class PicDevice : public StaticReceiver<PicDevice>
{
  enum ICW_MODE
  {
    ICW1,
    ICW2,
    ICW3,
    ICW4,
    OCW1 = 0,
  };
  enum
  {
    ICW1_IC4  = 0x01,
    ICW1_SNGL = 0x02,
    ICW1_LTIM = 0x08,
    ICW4_AEOI = 0x02,
    ICW4_MS   = 0x04,
    ICW4_BUF  = 0x08,
    ICW4_SFNM = 0x10,
  };

  DBus<MessageIrqLines>  &_bus_irq;
  DBus<MessagePic> 	 &_bus_pic;
  DBus<MessageLegacy> 	 &_bus_legacy;
  DBus<MessageIrqNotify> &_bus_notify;
  unsigned short _base;
  unsigned       _upstream_irq;
  unsigned short _elcr_base;
  unsigned char  _virq;
  unsigned char  _icw[4];
  ICW_MODE       _icw_mode;
  bool           _rotate_on_aeoi;
  bool           _smm;
  bool           _read_isr_reg;
  bool           _poll_mode;
  unsigned char  _prio_lowest;
  unsigned char  _imr;
  unsigned char  _isr;
  unsigned char  _irr;
  unsigned char  _elcr;
  unsigned char  _notify;

  // helper functions
  bool is_slave()                      { return (_icw[ICW4] & ICW4_BUF) ? (~_icw[ICW4] & ICW4_MS) : _virq; }
  void rotate_prios()                  { _prio_lowest = (_prio_lowest+1) & 7; }
  void specific_eoi(unsigned char irq) { _isr &= ~irq; propagate_irq(false); }
  void non_specific_eoi()
  {
    for (unsigned i=0; i<8; i++)
      {
	unsigned irq = 1 << ((_prio_lowest + 1 + i) & 7);
	if ((_isr & irq) && !(_smm && (~_imr & irq)))
	  {
	    specific_eoi(irq);
	    return;
	  }
      }
  }


  void reset_values()
  {
    _irr = 0;
    _imr = 0;
    _prio_lowest = 7;
    _smm = false;
    _read_isr_reg = false;
    if (~_icw[ICW1] & ICW1_IC4)
      _icw[ICW4] = 0;
    _icw[ICW3] = is_slave() ? 7 : 0;
    _isr = 0;         // no doc says this, but 2 machines tested
    _poll_mode = false;
    _elcr = (_icw[ICW1] & ICW1_LTIM) ? 0xff : 0;
    _notify = 0;
    propagate_irq(true);
  }


  /**
   * Check whether irqs will happen, optionally ACK irq and return the irq index.
   */
  bool prioritize_irq(unsigned char &irq_index, bool int_ack)
  {
    unsigned char tonotify = ~_irr & _notify;
    if (tonotify)
      {
	Cpu::atomic_and<unsigned char>(&_notify, ~tonotify);
	MessageIrqNotify msg(_virq, tonotify);
	_bus_notify.send(msg);
      }

    unsigned char state = _irr & ~_imr;
    for (unsigned i=0; i<8; i++)
      {
	irq_index = (_prio_lowest + 1 + i) & 7;
	unsigned irq = 1 << irq_index;

	if (!_smm && (_isr & irq) && !((_icw[ICW4] & ICW4_SFNM) && (_icw[ICW3] & irq)))
	  break;
	if (state & irq)
	  {
	    if (int_ack)
	      {
		_isr |= irq;
		if (~_elcr & irq)
		  Cpu::atomic_and<unsigned char>(&_irr, ~irq);
		if (_icw[ICW4] & ICW4_AEOI)
		  {
		    non_specific_eoi();
		    if (_rotate_on_aeoi) rotate_prios();
		  }
	      }
	    return true;
	  }
      }
    return false;
  }

  /**
   * Propagate an irq upstream.
   */
  void propagate_irq(bool send_deassert)
  {
    unsigned char dummy;
    if (prioritize_irq(dummy, false)) {
      if (!_virq) {
	MessageLegacy msg(MessageLegacy::INTR, 0);
	_bus_legacy.send(msg);
      }
      else {
	MessageIrqLines msg(MessageIrq::ASSERT_IRQ, _upstream_irq);
	_bus_irq.send(msg);
      }
    }
    else if (send_deassert && !_virq) {
      MessageLegacy msg(MessageLegacy::DEASS_INTR, 0);
      _bus_legacy.send(msg);
    }
  }


  /**
   * Upstream requests an irq vector.
   */
  void get_irqvector(unsigned char &res)
  {
    if (prioritize_irq(res, true)) {
	if (!is_slave() && (_icw[ICW3] & (1 << res))) {
	    MessagePic msg(res);
	    if (_bus_pic.send(msg)) {
	      res = msg.vector;
	      return;
	    }
	    Logging::printf("PicDevice::%s() spurious slave IRQ? for irr %x isr %x %x\n", __func__, _irr, _isr, res);
	}
    }
    else {
      Logging::printf("PicDevice::%s() spurious IRQ? for irr %x isr %x imr %x %x\n", __func__, _irr, _isr, res, _imr);
      res = 7;
    }
    res += _icw[ICW2];
    return;
  }

 public:

  /**
   * The CPU send an int-ack cycle?
   */
  bool  receive(MessageLegacy &msg)
  {
    if (msg.type != MessageLegacy::INTA) return false;
    unsigned char vec;
    get_irqvector(vec);
    msg.value = vec;
    propagate_irq(true);
    return true;
  }


  /**
   * We get an request on the three-wire PIC bus.
   */
  bool  receive(MessagePic &msg)
  {
    // get irq vector if slave and addr match
    if (is_slave() && (msg.slave == (_icw[ICW3] & 7))) {
      get_irqvector(msg.vector);
      propagate_irq(false);
      return true;
    }
    return false;
  }


  bool  receive(MessageIOIn &msg)
  {
    if (!in_range(msg.port, _base, 2) && msg.port != _elcr_base || msg.type != MessageIOIn::TYPE_INB)
      return false;

    if (msg.port == _elcr_base)
      msg.value = _elcr;
    else if (_poll_mode)
      {
	_poll_mode = false;
	unsigned char v = msg.value;
	if (prioritize_irq(v, true))  v |= 0x80;
	msg.value = v;
      }
    else
      if (msg.port == _base)
	msg.value = _read_isr_reg ? _isr : _irr;
      else
	msg.value = _imr;
    return true;
  }


  /**
   * Write to the PIC via IO ports.
   */
  bool  receive(MessageIOOut &msg)
  {
      if (!in_range(msg.port, _base, 2) && msg.port != _elcr_base || msg.type != MessageIOOut::TYPE_OUTB)
	return false;

      if (msg.port == _elcr_base)
	_elcr = msg.value;
      else if (msg.port == _base)
	{
	  if (msg.value & 0x10)
	    {
	      _icw[ICW1] = msg.value;
	      reset_values();
	      _icw_mode = ICW2;
	    }
	  else
	    if ((msg.value & 0x08) == 0)
	      {
		if (msg.value & 0x40)
		  {
		    if (msg.value & 0x20) specific_eoi(1 << (msg.value & 7));
		    if (msg.value & 0x80) _prio_lowest = msg.value & 7;
		  }
		else
		  if (msg.value & 0x20)
		    {
		      non_specific_eoi();
		      if (msg.value & 0x80) rotate_prios();
		    }
		  else
		    _rotate_on_aeoi = msg.value & 0x80;
	      }
	    else
	      {
		if (msg.value & 0x40) _smm = msg.value & 0x20;
		if (msg.value & 2)    _read_isr_reg = msg.value & 1;
		_poll_mode = msg.value & 4;
	      }
	}
      else
	{
	  switch (_icw_mode)
	    {
	    case ICW2:
	      _icw[ICW2] = msg.value & 0xf8;
	      _icw_mode = ICW3;
	      break;
	    case ICW3:
	      _icw_mode = ICW4;
	      if (~_icw[ICW1] & ICW1_SNGL)
		{
		  _icw[ICW3] = msg.value;
		  break;
		}
	    case ICW4:
	      _icw_mode = OCW1;
	      if (_icw[ICW1] & ICW1_IC4)
		{
		  _icw[ICW4] = msg.value;
		  break;
		}
	    case OCW1:
	      _imr = msg.value;
	      propagate_irq(true);
	      break;
	    default:
	      Logging::panic("invalid icw_mode: %x", _icw_mode);
	    }
	};
      return true;
    }


  /**
   * Raise the an irqline. This function needs to be multi-entrance
   * ready! It should only touch the _irr.
   */
  bool  receive(MessageIrqLines &msg)
    {
      if (in_range(msg.line, _virq, 8))
	{
	  unsigned char irq = 1 << (msg.line - _virq);
	  if (msg.type == MessageIrq::ASSERT_NOTIFY)
	      Cpu::atomic_or(&_notify, irq);

	  if (msg.type == MessageIrq::DEASSERT_IRQ)
	    {
	      if ((_irr & irq) && (_elcr & irq))
		{
		  Cpu::atomic_and<unsigned char>(&_irr, ~irq);
		  propagate_irq(true);
		}
	    }
	  else if (!(_irr & irq))
	    {
	      if (msg.line == 0) COUNTER_INC("pirq0"); else COUNTER_INC("pirqN");

	      Cpu::atomic_or(&_irr, irq);
	      propagate_irq(false);
	    }
	  return true;
	}
      return false;
    }


 PicDevice(DBus<MessageIrqLines> &bus_irq, DBus<MessagePic> &bus_pic, DBus<MessageLegacy> &bus_legacy, DBus<MessageIrqNotify> &bus_notify,
	   unsigned short base, unsigned char irq, unsigned short elcr_base, unsigned char virq) :
   _bus_irq(bus_irq), _bus_pic(bus_pic), _bus_legacy(bus_legacy), _bus_notify(bus_notify),
   _base(base), _upstream_irq(irq), _elcr_base(elcr_base), _virq(virq), _icw_mode(OCW1)
  {
    _icw[ICW1] = 0;
    reset_values();
 }
};


PARAM_HANDLER(pic,
	      "pic:iobase,(irq),(elcr) - Attach an PIC8259 at the given iobase.",
	      "Example: 'pic:0x20,,0x4d0 pic:0xa0,2'",
	      "The first PIC is always the master. An irq can be given when creating",
	      "a slave pic.  The irqlines are automatically distributed, such that",
	      "the first pic gets 0-7, the second one 8-15,... The optional elcr",
	      "parameter specifies the io address of the ELCR register")
{
  static unsigned virq;
  PicDevice *dev = new PicDevice(mb.bus_irqlines,
				 mb.bus_pic,
				 mb.bus_legacy,
				 mb.bus_irqnotify,
				 argv[0],
				 argv[1],
				 argv[2],
				 virq);
  mb.bus_ioin.    add(dev, PicDevice::receive_static<MessageIOIn>);
  mb.bus_ioout.   add(dev, PicDevice::receive_static<MessageIOOut>);
  mb.bus_irqlines.add(dev, PicDevice::receive_static<MessageIrqLines>);
  mb.bus_pic.     add(dev, PicDevice::receive_static<MessagePic>);
  if (!virq)
    mb.bus_legacy.add(dev, PicDevice::receive_static<MessageLegacy>);
  virq += 8;
}

