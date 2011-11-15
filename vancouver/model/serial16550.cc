/** @file
 * UART 16550 emulation.
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
 * Implements a 16550 UART.
 *
 * State: stable
 * Missing Features:
 *  * no write fifo
 *  * no transmission effect of stopbit+parity+divisor
 *  * no character timeout indication -> need a timer for that
 *  * no MSR setting via client
 *  * no HW reset support
 * Ignored bits: FCR2-3, LCR2-6, LSR2-4,7
 * Documentation: NSC 16550D - PC16550D.pdf
 */
class SerialDevice : public StaticReceiver<SerialDevice>, public DiscoveryHelper<SerialDevice>
{
public:
  Motherboard &_mb;
private:
  unsigned short _base;
  unsigned char _irq;
  unsigned _hostserial;
  static const unsigned FIFOSIZE = 16;
  enum {
    RBR = 0,
    THR = 0,
    IER,
    FCR,
    IIR = FCR,
    LCR,
    MCR,
    LSR,
    MSR,
    SCR,
    DLL,
    DLM,
    MAX,
  };
  unsigned char _regs[MAX];
  unsigned char _rfifo[FIFOSIZE];
  unsigned char _rfpos;
  unsigned char _rfcount;
  unsigned char _triggerlevel;
  unsigned char _sendmask;

  /**
   * Returns the IIR and thereby prioritize the interrupts.
   */
  unsigned char get_iir()
  {
    unsigned char value = 1;
    if (_regs[IER] & 8 && _regs[MSR] & 0xf)  value = 0;
    if (_regs[IER] & 2 && _regs[LSR] & 0x20) value = 2;
    if (_regs[IER] & 1
	&& ((~_regs[FCR] & 1 && _regs[LSR] & 1)
	    || (_triggerlevel <= _rfcount)))
      value = 4;
    if (_regs[IER] & 4 && _regs[LSR] & 0x1e) value = 6;
    if (_regs[FCR] & 1)  value |= 0xc;
    return value;
  }

  void update_irq()
  {
    MessageIrqLines msg(MessageIrq::ASSERT_IRQ, _irq);
    if (~get_iir() & 1 && _regs[MCR] & 8)
      _mb.bus_irqlines.send(msg);
  }


public:
  bool  receive(MessageSerial &msg)
  {
    if (msg.serial != _hostserial)   return false;

    unsigned char or_lsr;
    if (_regs[FCR] & 1)
      // fifo mode
      {
	if (_rfcount >= FIFOSIZE)
	  {
	    or_lsr = 3;
	  }
	else
	  {
	    _rfifo[_rfpos] = msg.ch;
	    _rfpos = (_rfpos+1) % FIFOSIZE;
	    or_lsr = 1;
	    _rfcount++;
	  }
      }
    else
      {
	or_lsr = _regs[LSR] & 1 ? 3 : 1;
	_regs[RBR] =msg.ch;
      }
    _regs[LSR] |= or_lsr;
    update_irq();
    return true;
  }


  bool  receive(MessageIOIn &msg)
  {
    if (!in_range(msg.port, _base, 8) || msg.type != MessageIOIn::TYPE_INB)
      return false;
    unsigned offset = msg.port - _base;
    if (_regs[LCR] & 0x80 && offset <= IER)
      offset += DLL - THR;

    msg.value = _regs[offset];
    switch (offset)
      {
      case RBR:
	if (_regs[FCR] & 1)
	  {
	    msg.value = _rfifo[(_rfpos - _rfcount) % FIFOSIZE];
	    if (_rfcount) _rfcount--;
	  }
	else
	  msg.value = _regs[RBR];
	if (~_regs[FCR] & 1 || !_rfcount) _regs[LSR] &= ~1;
	break;
      case IIR:
	msg.value = get_iir();
	break;
      case LSR:
	// clear all error error indications
	_regs[LSR] &= 0x61;
	break;
      case IER:
      case MCR:
      case LCR:
      case MSR ... DLM:
	break;
      default:
	Logging::panic("SerialDevice::%s() %x", __func__, msg.port);
      }
    update_irq();
    return true;
  }


  bool  receive(MessageIOOut &msg)
  {
    if (!in_range(msg.port, _base, 8) || msg.type != MessageIOOut::TYPE_OUTB)
      return false;

    msg.value &= 0xff;
    unsigned offset = msg.port - _base;
    if (_regs[LCR] & 0x80 && offset <= IER)
      offset += DLL - THR;

    switch (offset)
      {
      case THR:
	{
	  MessageSerial msg2(_hostserial, msg.value & _sendmask);
	  if (_regs[MCR] & 0x10)
	    // loopback
	    receive(msg2);
	  else
	    {
	      // write directly, no write fifo here
	      msg2.serial++;
	      _mb.bus_serial.send(msg2);
	    }
	}
	break;
      case IER:
	_regs[offset] = msg.value & 0xf;
	break;
      case FCR:
	if ((_regs[FCR] ^ msg.value) & 1 || ((msg.value & 3) == 3))
	  {
	    // clear fifos
	    _rfcount = 0;
	    _regs[LSR] = 0x60;
	  }

	if (msg.value & 1)
	  {
	    unsigned char level[] = {1, 4, 8, 14};
	    _triggerlevel = level[(msg.value >> 6) & 3];
	  }
	_regs[FCR] = msg.value;
	break;
      case LCR:
	_regs[LCR] = msg.value;
	_sendmask = (1 << (5 + (msg.value & 3))) -1;
	break;
      case MCR:
	_regs[MCR] = msg.value & 0x1f;
	if (msg.value & 0x10)
	  {
	    unsigned char input = ((msg.value & 0x1) << 1) | ((msg.value & 0x2) >> 1) | (msg.value & 0xc);
	    _regs[MSR] =  (input << 4) | (((_regs[MSR] >> 4) ^ input) & ~(input & 4));
	  }
	else
	  _regs[MSR] = 0xb0;
	break;
      case LSR:
	if (_regs[FCR] & 1)
	  msg.value &= _rfcount ? 0x1e : 0x2;
	else
	  msg.value &= 0x1f;
      case SCR ... DLM:
	_regs[offset] = msg.value;
	break;
      default:
	Logging::panic("SerialDevice::%s() %x %x", __func__, msg.port, msg.value);
      }
    update_irq();
    return true;
  }


  void discovery() {

    unsigned installed_hw = ~0u;
    check0(!discovery_read_dw("bda", 0x10, installed_hw));
    unsigned ioports      = (installed_hw >> 9) & 0x7;
    if (ioports < 4) {

      discovery_write_dw("bda", ioports * 2, _base, 2);
      ioports++;
      discovery_write_dw("bda", 0x10, (installed_hw & 0xfffff1ff) | (ioports << 9), 4);
    }
  }


  SerialDevice(Motherboard &mb, unsigned short base, unsigned char irq, unsigned hostserial)
    : _mb(mb), _base(base), _irq(irq), _hostserial(hostserial), _rfcount(0), _triggerlevel(1), _sendmask(0x1f)
    {
      memset(_regs, 0, sizeof(_regs));
      _regs[LSR] = 0x60;
      _regs[MSR] = 0xb0;
      _mb.bus_ioin.     add(this, receive_static<MessageIOIn>);
      _mb.bus_ioout.    add(this, receive_static<MessageIOOut>);
      _mb.bus_serial.   add(this, receive_static<MessageSerial>);
      _mb.bus_discovery.add(this, discover);
    }
};


PARAM_HANDLER(serial,
	      "serial:iobase,irq,hdev -  attach a virtual serial port that should use the given hostdev as backend.",
	      "Example: 'serial:0x3f8,8,0x47'.",
	      "The input comes from hdev and the output is redirected to hdev+1.")
{
  new SerialDevice(mb, argv[0], argv[1], argv[2]);
}
