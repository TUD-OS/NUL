/** @file
 * PIT8254 emulation.
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
 * A single counter of a PIT.
 *
 * State: stable
 * Implementation Note: the access to the _modus variable is not SMP safe.
 * Documentation: Intel 82c54 - intel-82c54-timer.pdf
 */
class PitCounter : public StaticReceiver<PitCounter>
{
  friend class PitTest;

  enum Modes
  {
    BCD          = 0x001,
    RW_LOW       = 0x010,
    RW_HIGH      = 0x020,
    NULL_COUNT   = 0x040,
  };
  enum Features
  {
    FPERIODIC               = 1 << 0,
    FSOFTWARE_TRIGGER       = 1 << 1,
    FGATE_DISABLE_COUNTING  = 1 << 2,
    FSQUARE_WAVE            = 1 << 3,
    FCOUNTDOWN              = 1 << 4,
  };

  unsigned short _modus;
  unsigned short _latch;
  unsigned short _new_counter;
  unsigned       _initial;
  unsigned char  _latched_status;
  struct {
    unsigned char _read_low   : 1;
    unsigned char _wrote_low  : 1;
    unsigned char _stopped    : 1;
    unsigned char _stopped_out: 1;
    unsigned char _gate       : 1;
    unsigned char _lstatus    : 1;
    unsigned char _latched    : 2;
  };
  timevalue            _start;
  DBus<MessageTimer> * _bus_timer;
  DBus<MessageIrqLines> * _bus_irq;
  unsigned             _irq;
  Clock                _clock;
  unsigned             _timer;
  static const long FREQ = 1193180;

  bool feature(Features f)
  {
    int features[6] = { FCOUNTDOWN | FSOFTWARE_TRIGGER | FGATE_DISABLE_COUNTING,
			FCOUNTDOWN,
			FPERIODIC | FGATE_DISABLE_COUNTING,
			FPERIODIC | FGATE_DISABLE_COUNTING | FSQUARE_WAVE,
			FSOFTWARE_TRIGGER | FGATE_DISABLE_COUNTING,
			0};
    return (features[(_modus >> 1) & 0x7] & f) == f;
  };

  unsigned short s2bcd(unsigned short value)
  {
    if (_modus & BCD)
      value = (((value / 1000) % 10) << 12) + (((value / 100) % 10) << 8) + (((value / 10) % 10) << 4) +  value % 10;
    return value;
  }

  unsigned short bcd2s(unsigned short value)
  {
    if (_modus & BCD)
      value =  ((value & 0xf000) >> 12) * 1000 + ((value & 0xf00) >> 8) * 100 + ((value & 0xf0) >> 4) * 10 + (value & 0xf);
    return value;
  }


  void load_counter()
    {
      _initial = _new_counter ? _new_counter : 65536;
      _modus &= ~NULL_COUNT;
    }


  void disable_counting()
  {
    _latch = get_counter();
    _stopped_out = feature(FPERIODIC) || get_out();
    _start = _clock.clock(FREQ);
    _stopped = 1;
  }


  /**
   * Rearm a new timeout.
   */
  void update_timer()
  {
    if (_irq == ~0U)  return;
    timevalue t = _clock.clock(FREQ);
    timevalue to= _start;
    if (feature(FPERIODIC))
      to = t + (_initial + _start - t) % _initial;
    MessageTimer msg(_timer, _clock.abstime((to < t) ? 0 : (to - t), FREQ));
    _bus_timer->send(msg);
  }


  /**
   * Get the current counter value.
   */
  unsigned short get_counter()
  {
    if (_stopped)  return _latch;

    long long res = _start - _clock.clock(FREQ);
    if (_modus & BCD) res = (res % 10000);

    // are we still having an old value?
    if (_start - _initial - 1 == _clock.clock(FREQ))
      return _latch;

    if (res <= 0)  load_counter();
    if (feature(FPERIODIC) && (res <= 0)) res = _initial + (res % _initial);
    if (feature(FSQUARE_WAVE))            res = ((res*2 % _initial+1) + (~_initial & 1)) & ~1;
    if (_modus & BCD)                     res = 10000 + (res % 10000);
    return res;
  }

  void reload_counter()
  {
    _latch = get_counter();
    _stopped_out = (_modus & 0xe) != 0 ? get_out() : 0;
    _stopped = 0;
    _start = _clock.clock(FREQ) + _new_counter + 1;
    load_counter();
    update_timer();
  }

 public:

  void read_back(unsigned char value)
  {
    // sync state
    unsigned short counter = get_counter();
    if (!(value & 0x20) && !_lstatus)
      {
	_latched_status = (_modus & 0x7f) | (get_out() << 7);
	_lstatus = 1;
      }
    if (!(value & 0x10) && !_latched)
      {
	_latch = counter;
	_latched = ((_modus & (RW_LOW | RW_HIGH))/RW_LOW);
      }
  }


  /**
   * Set the modus of the counter.
   */
  void set_modus(unsigned char modus)
  {
    if (modus & (RW_LOW | RW_HIGH))
      {
	if ((modus & 0xe) <= 10)
	  {
	    _modus = (_modus & ~0x3f) | (modus & 0x3f) | NULL_COUNT;
	    _start = 0;
	    disable_counting();
	    _stopped_out = (_modus & 0xe) != 0;
	    _wrote_low = 0;
	    _read_low = 0;
	    _lstatus = 0;
	    _latched = 0;
	  }
      }
    else
      read_back(0x20);
  }


  /**
   * Manipulate the gate.
   */
  void set_gate(unsigned char value)
  {
    if (value == _gate)  return;
    _gate = value;

    if (!value && feature(FGATE_DISABLE_COUNTING))  disable_counting();
    if (value)
      {
	_stopped_out = get_out();
	if (!feature(Features(FSOFTWARE_TRIGGER)))
	  reload_counter();
	else if (_stopped)
	  {
	    _initial = _latch ? _latch : 65536;
	    _start = _clock.clock(FREQ) + _initial + 1;
	    _stopped = 0;
	  }
      }
  }

  /*
   * Returns true if out is high;
   */
  bool get_out()
  {
    if (_stopped || _start - _initial -1 == _clock.clock(FREQ))
      return _stopped_out;

    if (feature(FCOUNTDOWN))
      return _clock.clock(FREQ) >= _start;
    if (feature(FPERIODIC))
      if (!feature(FSQUARE_WAVE))
	return get_counter() != 1;
      else
	return ((_clock.clock(FREQ) - _start + _initial) % _initial)*2 < _initial;
    return _clock.clock(FREQ) != _start;
  }

  /**
   * Read from the counter port.
   */
  unsigned char read()
  {
    unsigned char value;
    if (_lstatus)
      {
	value = _latched_status;
	_lstatus = 0;
      }
    else if (_latched & 1)
      {
	value = s2bcd(_latch) & 0xff;
	_latched--;
      }
    else if (_latched & 2)
      {
	value = (s2bcd(_latch) >> 8) & 0xff;
	_latched = 0;
      }
    else if (_read_low)
      {
	value = (s2bcd(get_counter()) >> 8) & 0xff;
	_read_low = 0;
      }
    else
      {
	if (_modus & RW_LOW)
	  value = s2bcd(get_counter()) & 0xff;
	else
	  value = (s2bcd(get_counter()) >> 8) & 0xff;
	if ((_modus & (RW_LOW | RW_HIGH)) == (RW_LOW | RW_HIGH))
	  _read_low = 1;
      }
    return value;
  }

  /**
   * Write to the counter port.
   */
  void write(unsigned char value)
  {
    if (_wrote_low)
      {
	_wrote_low = 0;
	_new_counter =  (_new_counter & 0xff) + bcd2s((value & 0xff) << 8);
      }
    else if (_modus & RW_LOW)
      {
	if (_modus & RW_HIGH)
	  {
	    _wrote_low = 1;
	    if (feature(Features(FSOFTWARE_TRIGGER | FCOUNTDOWN)) && !get_out()) disable_counting();
	  }
	_new_counter = bcd2s(value) & 0xff;
	if (_modus & RW_HIGH)
	  return;
      }
    else
      _new_counter = bcd2s((value & 0xff) << 8);

    _modus |= NULL_COUNT;
    if (feature(FSOFTWARE_TRIGGER))
      {
	if (_gate)
	  reload_counter();
	else
	  _latch = _new_counter;
      }
    else
      if (feature(FPERIODIC) && _gate)
	{
	  if (_stopped)
	    reload_counter();
	  else
	    _start = _clock.clock(FREQ) + get_counter();
	}
  }


  bool  receive(MessageIrqNotify &msg)
  {
    if (msg.baseirq != (_irq & ~7) || !(msg.mask & (1 << (_irq & 7)))) return false;
    if (feature(FPERIODIC))  update_timer();
    return true;
  }


  bool  receive(MessageTimeout &msg)
  {
    if (msg.nr == _timer)
      {
	// a timeout has triggerd
	MessageIrqLines msg1(MessageIrq::ASSERT_NOTIFY, _irq);
	_bus_irq->send(msg1);
	return true;
      }
    return false;
  }


  PitCounter(DBus<MessageTimer> *bus_timer, DBus<MessageIrqLines> *bus_irq, unsigned irq, Clock *clock)
    : _start(0), _bus_timer(bus_timer), _bus_irq(bus_irq), _irq(irq), _clock(*clock), _timer(0)
  {
    assert(_clock.freq() != 0);
    if (_irq != ~0U)
      {
	MessageTimer msg0;
	if (!_bus_timer->send(msg0))
	  Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);
	_timer = msg0.nr;
      };
  }
  PitCounter() : _clock(0) {}
};


/**
 * An implementation of the Intel 8254.
 *
 * State: stable
 */
class PitDevice : public StaticReceiver<PitDevice>
{
  friend class PitTest;
  unsigned short _base;
  unsigned       _addr;
  static const unsigned COUNTER = 3;
  PitCounter _c[COUNTER];

 public:

  bool  receive(MessagePit &msg)
  {
    if (!in_range(msg.pit, _addr, COUNTER)) return false;
    switch (msg.type)
      {
      case MessagePit::GET_OUT:
	msg.value = _c[msg.pit - _addr].get_out();
	break;
      case MessagePit::SET_GATE:
	_c[msg.pit - _addr].set_gate(msg.value);
	break;
      default:
	assert(0);
      }
    return true;
  }


 bool  receive(MessageIOIn &msg)
 {
   if (!in_range(msg.port, _base, COUNTER) || msg.type != MessageIOIn::TYPE_INB)
     return false;
   msg.value = _c[msg.port - _base].read();
   return true;
 }


 bool  receive(MessageIOOut &msg)
 {
   if (!in_range(msg.port, _base, COUNTER+1) || msg.type != MessageIOOut::TYPE_OUTB)
     return false;
   if (msg.port == _base + COUNTER)
     {
       if ((msg.value & 0xc0) == 0xc0)
	 {
	   for (unsigned i=0; i < COUNTER; i++)
	     if (msg.value & (1<<(i+1)))
	       {
		 _c[i].read_back(msg.value);
	       }
	 }
       else
	 _c[(msg.value >> 6) & 3].set_modus(msg.value);
       return true;
     }
   _c[msg.port - _base].write(msg.value);
   return true;
 }


  PitDevice(Motherboard &mb, unsigned short base, unsigned irq, unsigned pit)
    : _base(base), _addr(pit*COUNTER)
  {
    for (unsigned i=0; i < COUNTER; i++)
      {
	_c[i] = PitCounter(&mb.bus_timer, &mb.bus_irqlines, i ? ~0U : irq, mb.clock());
	if (!i) mb.bus_irqnotify.add(&_c[i], PitCounter::receive_static<MessageIrqNotify>);
	if (!i) mb.bus_timeout.add(&_c[i],   PitCounter::receive_static<MessageTimeout>);
	_c[i].set_gate(1);
      }
  }
};


PARAM_HANDLER(pit,
	      "pit:iobase,irq - attach a PIT8254 to the system.",
	      "Example: 'pit:0x40,0'")
{
  static unsigned pit_count;
  PitDevice *dev = new PitDevice(mb,
				 argv[0],
				 argv[1],
				 pit_count++);

  mb.bus_ioin.add(dev,  PitDevice::receive_static<MessageIOIn>);
  mb.bus_ioout.add(dev, PitDevice::receive_static<MessageIOOut>);
  mb.bus_pit.add(dev,   PitDevice::receive_static<MessagePit>);
} 
