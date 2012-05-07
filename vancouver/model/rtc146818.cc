/** @file
 * MC146818A Realtime Clock and CMOS emulation.
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
#include "service/time.h"
#include "service/bcd.h"

using namespace Bcd;

/**
 * Device model for the MC146818 realtime clock.
 *
 * State: testing
 * Features: 128byte RAM, gettime, updatetime, UIE, alarm, periodic-irqs, divider
 * Missing: daylight-saving
 * Unused:  sqw-output
 * Documentation: rtc82885.pdf, MC146818AS.pdf, Intel ICH{3-7} documentation
 */
class Rtc146818 : public StaticReceiver<Rtc146818>
{
  friend class RtcTest;
  DBus<MessageTimer>    &_bus_timer;
  DBus<MessageIrqLines> &_bus_irqlines;
  Clock                *_clock;
  unsigned              _timer;
  unsigned short        _iobase;
  unsigned              _irq;
  unsigned char         _index;
  unsigned char         _ram[128];
  timevalue             _offset;
  timevalue             _last;

  /**
   * Timing:
   *   1. seconds are updated at us == 0
   *   2. UIP is set from [FREQ-tBUC-tUC, FREQ-1]
   *   3. the update happens from [FREQ-tUC, FREQ-1]
   *   4. the periodic IRQ happens at (period/2, 3*period/2,) ...
   */
  enum {
    FREQ    = 32768,
    tBUC    =     8,   //~244  usec
    tUC     =    65,   //~1984 usec; VIA uses 1708usec - 56 instead...
  };

  unsigned char convert_bcd(unsigned char value)
  {
    if (~_ram[0xb] & 0x4) to_bcd(value);
    return value;
  }

  /**
   * Read values from RAM and convert time information from BCD and
   * 12h format.
   */
  unsigned char ram(unsigned char index)
  {
    assert(index < sizeof(_ram));
    assert(index < 0xa || index == 0x32);

    unsigned char value = _ram[index];
    if (!index)  value &= 0x7f;
    if (~_ram[0xb] & 0x4) from_bcd(value);
    if ((index == 4 || index == 5) && ~_ram[0xb] & 2)
      {
	value = _ram[index] & 0x7f;
	if (~_ram[0xb] & 0x4) from_bcd(value);
	value %= 12;
	if (_ram[index] & 0x80) value += 12;
      }
    return value;
  }

  int get_divider()
  {
    int divider = 25 - ((_ram[0xa] >> 4) & 7) * 5;
    if (divider >= 22)  divider = 22;
    return divider;
  }


  timevalue get_counter()
  {
    timevalue value = _clock->clock(1 << 30);
    // scale the counter with the divider
    int divider = get_divider();
    if (divider < 0)  return 0;
    value >>= divider;
    return value - _offset;
  }


  unsigned get_periodic_tics()
  {
    if (_ram[0xa] & 0xf && (_ram[0xa] & 0x60) != 0x60)
      {
	unsigned tics = 1;
	tics <<= ((_ram[0xa] & 0xf) - 1);
	if (tics < 3) tics <<= 7;
	return tics;
      }
    return 0;
  }


  /**
   * Set the IRQ flags and raise an IRQ if needed.
   */
  void set_irqflags(unsigned char value)
  {
    unsigned oldvalue = _ram[0xc];
    _ram[0xc] = value;
    if ((_ram[0xb] & _ram[0xc]) & 0x70)  _ram[0xc] |= 0x80;
    if (( oldvalue ^ _ram[0xc]) & 0x80)
      {
	MessageIrqLines msg((_ram[0xc] & 0x80) ? MessageIrq::ASSERT_NOTIFY : MessageIrq::DEASSERT_IRQ, _irq);
	_bus_irqlines.send(msg);
      }
  }

  /**
   * Return the time from the RAM locations.
   */
  timevalue get_ram_time()
  {
    struct tm_simple tm;
    tm.sec  = ram(0);
    tm.min  = ram(2);
    tm.hour = ram(4);
    tm.mday = ram(7);
    tm.mon  = ram(8);
    tm.year = ram(9);
    tm.year+= ram(0x32) * 100;
    return mktime(&tm);
  }

  /**
   * Update the time with the given seconds.
   */
  void update_ram(timevalue seconds)
  {
    struct tm_simple tm;
    gmtime(seconds, &tm);
    _ram[0] = convert_bcd(tm.sec);
    _ram[2] = convert_bcd(tm.min);
    _ram[4] = convert_bcd(tm.hour);
    if (~_ram[0xb] & 2)
      {
	unsigned char pm = (tm.hour >= 12) << 7;
	tm.hour %= 12;
	if (!tm.hour) tm.hour = 12;
	_ram[4] = pm | convert_bcd(tm.hour);
      }
    _ram[6] = convert_bcd(tm.wday);
    _ram[7] = convert_bcd(tm.mday);
    _ram[8] = convert_bcd(tm.mon);
    _ram[9] = convert_bcd(tm.year % 100);
    _ram[0x32] = convert_bcd(tm.year / 100);

    // update ended, set flag
    set_irqflags(_ram[0xc] | 0x10);
  }


  /**
   * Returns the time of the next alarm in seconds.
   */
  timevalue next_alarm(timevalue seconds)
  {
    /*
     * The algorithm to find the next alarm is quite tricky.
     *
     * The variables are as follow:
     * period    - the period when the alarm repeats
     * start     - the start time of the alarm, always less than the period
     * wildcards - a bitfield indicating which component of the alarm (h, min, sec) is a wildcard
     * subranges - the size of a subrange in the period where the alarm fires more often.
     * now       - the current time broken down to the period
     */
    static const unsigned   periods[8] = { 86400, 86400, 86400, 86400, 3600, 3600, 60, 1};
    static const unsigned subranges[8] = {     0,    59, 59*60,  3599,    0,   59,  0, 0};
    unsigned start = 0;
    unsigned wildcards = 0;
    unsigned p = 1;
    for (unsigned i=0; i < 3; i++)
      {
	if (_ram[i*2 + 1] >= 0xc0)
	  wildcards |= 1 << i;
	else
	  {
	    unsigned v = ram(i*2 + 1);
	    if (v >= 60 || (i==2 && v >= 24))  return ~0ull;
	    start += p*v;
	  }
	p *= 60;
      }
    unsigned period = periods[wildcards];

    // we are interested in an alarm at least a second in the future
    seconds++;

    // split the current time in multiple of periods (time) and the remainder (now)
    unsigned now = Math::div64(seconds, period);

    // is the next alarm in the future?
    if (now <= start)  return seconds * period + start;

    // are we hit the subrange?
    if (now <= (start + subranges[wildcards]))
      {
        if (wildcards == 2)
	  {
	    /*
	     * With a wildcard only in the minute, the subrange gets a
	     * granularity of 60 seconds. Thus we have to infer the
	     * seconds from the alarm time.
	     */
            if (now % 60 > start % 60)  now += 60;
	    now += (start % 60 - now % 60);
	  }
	return seconds * period + now;
      }
    // the alarm is in the next period
    return (seconds + 1) * period + start;
  }


  /**
   * Performs an update cycle and updates the time in the RAM.
   */
  unsigned update_cycle(timevalue now)
  {
    timevalue seconds = now;

    unsigned  fnow  = seconds % FREQ;
    if ((_ram[0xa] & 0x60) == 0x60) return fnow;

    unsigned  flast = _last % FREQ;
    unsigned  periodic_tics = get_periodic_tics();

    if (periodic_tics && ((fnow - periodic_tics/2) / periodic_tics) != ((flast - periodic_tics/2) / periodic_tics))
      set_irqflags(_ram[0xc] | 0x40);
    seconds /= FREQ;

    // update cycle if not SET and not in the very same second
    if (~_ram[0xb] & 0x80 && seconds != (_last / FREQ))
      {
	timevalue last_seconds = get_ram_time();
	seconds = last_seconds + seconds - (_last / FREQ);
	update_ram(seconds);
	if (next_alarm(last_seconds) <= seconds)  set_irqflags(_ram[0xc] | 0x20);
      }
    _last = now;
    return fnow;
  };


  /**
   * Reprogram the next timer.
   */
  void update_timer(timevalue last_seconds, timevalue now)
  {
    timevalue next = 0;
    unsigned periodic_tics = get_periodic_tics();
    if (_ram[0xb] & 0x40 && periodic_tics)
      next = periodic_tics - (static_cast<unsigned>(now % FREQ) + periodic_tics/2) % periodic_tics;
    else if (_ram[0xb] & 0x10)
      next = FREQ - now % FREQ;
    else if (_ram[0xb] & 0x20)
      {
	timevalue alarm = next_alarm(last_seconds);
	if (alarm != ~0ull) next = (alarm - last_seconds) * FREQ - now % FREQ;
      }
    if (next)
      {
	// scale the next timeout with the divider
	int divider = get_divider();
	if (divider < 0)  return;
	if (divider >= 15) next = _clock->abstime(next, FREQ >> (divider - 15));
	else               next = _clock->abstime(next, FREQ << (15 - divider));

	MessageTimer msg(_timer, next);
	_bus_timer.send(msg);
      }
  }


public:
  void reset(MessageTime &time)
  {

    memset(_ram, 0, sizeof(_ram));
    _ram[0xa] = 0x26; // 32K base freq, periodic: 1kz
    _ram[0xb] = 0x02; // 24h+BCD mode
    _ram[0xd] = 0x80; // Valid RAM and Time

    timevalue now = Math::muldiv128(time.wallclocktime - time.timestamp, FREQ, Config::WALLCLOCK_FREQUENCY);
    update_ram(now / FREQ);
    set_irqflags(0);
    _offset = _last = 0;
  }


  bool  receive(MessageIOIn &msg)
  {
    if (!in_range(msg.port, _iobase, 8) || msg.type != MessageIOIn::TYPE_INB)
      return false;
    timevalue now = get_counter();
    unsigned mod = update_cycle(now);
    if (msg.port & 1)
      {
	// the registers are not available during an update cycle!
	if ((mod >= (FREQ - tUC)) && _index < 10)  return false;
	msg.value =  _ram[_index];

	switch (_index)
	  {
	  case 0:
	    msg.value &= 0x7f;
	    break;
	  case 0xa:
	    msg.value &= 0x7f;
	    // XXX what if SET=1?
	    if (~_ram[0xb] & 0x80 && mod >= (FREQ - tBUC - tUC)) msg.value |= 0x80;
	    break;
	  case 0xc:
	    set_irqflags(0);
	    update_timer(get_ram_time(), now);
	    break;
	  default:
	    break;
	  }
      }
    else
      // Port 0x70 is not readable. See RBL+ICH docu. Tested on ICH10.
      return false;

    return true;
  }


  bool  receive(MessageIOOut &msg)
  {
    if (!in_range(msg.port, _iobase, 8) || msg.type != MessageIOOut::TYPE_OUTB)
      return false;
    if (msg.port & 1)
      {
	timevalue now = get_counter();
	update_cycle(now);
	switch (_index)
	  {
	  case 0x0:
	    _ram[_index] = _ram[_index] & 0x80 | msg.value & 0x7f;
	    break;
	  case 0xa:
	    {
	      bool toggled_reset = ((_ram[0xa] & 0x60) == 0x60) && ((msg.value & 0x60) != 0x60);
	      _ram[_index] = msg.value;
	      if (toggled_reset)
		{
		  // switch from reset to non-reset mode, the next update is a half second later...
		  _offset  = _clock->clock(FREQ) - FREQ/2;
		  _last    = FREQ/2; // to make sure the periodic updates are right!
		}
	    }
	    break;
	  case 0xc:
	  case 0xd:
	    break; // readonly
	  case 0x0b:
	    // enabled counting?
	    if ((_ram[0xb] & 0x80) && ~msg.value & 0x80)
	      // skip missed updates, but do not reset the divider chain
	      _last = _clock->clock(FREQ);
	    // Fallthrough
	  default:
	    _ram[_index] = msg.value;
	  }
	if (_index < 0xc)  update_timer(get_ram_time(), get_counter());
	if (_index == 0xb) set_irqflags(_ram[0xc]);
      }
    else
      _index = msg.value & 0x7f;
    return true;
  }


  bool  receive(MessageIrqNotify &msg)
  {
    if (msg.baseirq != (_irq & ~7) || !(msg.mask & (1 << (_irq & 7)))) return false;
    update_timer(get_ram_time(), get_counter());
    return true;
  }


  bool  receive(MessageTimeout &msg)
  {
    if (msg.nr != _timer) return false;
    update_cycle(get_counter());
    return true;
  }


  Rtc146818(DBus<MessageTimer> &bus_timer, DBus<MessageIrqLines> &bus_irqlines, Clock *clock, unsigned timer, unsigned short iobase, unsigned irq)
    : _bus_timer(bus_timer), _bus_irqlines(bus_irqlines), _clock(clock), _timer(timer), _iobase(iobase), _irq(irq)
  {}
};

PARAM_HANDLER(rtc,
	      "rtc:iobase,irq - Attach a realtime clock including its CMOS RAM.",
	      "Example: 'rtc:0x70,8'")
{
  MessageTimer msg0;
  if (!mb.bus_timer.send(msg0))
    Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);

  Rtc146818 *rtc = new Rtc146818(mb.bus_timer, mb.bus_irqlines, mb.clock(), msg0.nr, argv[0],argv[1]);
  MessageTime msg1;
  if (!mb.bus_time.send(msg1))
    Logging::printf("could not get wallclock time!\n");
  rtc->reset(msg1);
  mb.bus_ioin.     add(rtc, Rtc146818::receive_static<MessageIOIn>);
  mb.bus_ioout.    add(rtc, Rtc146818::receive_static<MessageIOOut>);
  mb.bus_timeout.  add(rtc, Rtc146818::receive_static<MessageTimeout>);
  mb.bus_irqnotify.add(rtc, Rtc146818::receive_static<MessageIrqNotify>);
}

