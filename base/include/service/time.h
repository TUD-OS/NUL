/** @file
 * Standard include file: time.h
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

#pragma once

#include "math.h"

/**
 * An appreviated broken down time.
 */
struct tm_simple {
  int sec;
  int min;
  int hour;
  int mday;
  int mon;
  int year;
  int wday;
  int yday;
  int isdsdt;
  tm_simple() {};
  tm_simple(int _year, int _mon, int _mday, int _hour, int _min, int _sec) : sec(_sec), min(_min), hour(_hour), mday(_mday), mon(_mon), year(_year), wday(0), yday(0), isdsdt(0) {}
};

static inline bool is_leap(int year) { return (!(year & 3) && ((year % 100) != 0)) || (year % 400 == 0); }

static inline timevalue mktime(struct tm_simple *tm)
{
  bool before_march = tm->mon < 3;
  timevalue days = ((tm->mon + 10)*367)/12 + 2*before_march - 719866 + tm->mday - before_march * is_leap(tm->year) + tm->year*365;
  days += tm->year / 4 - tm->year / 100 + tm->year / 400;
  return ((days*24+tm->hour)*60+tm->min)*60 + tm->sec;
}

static inline unsigned moddiv(timevalue &value, unsigned divider)
{
  unsigned mod = Math::div64(value, divider);
  unsigned d = value;
  value = mod;
  return d;
}

static inline void gmtime(timevalue seconds, struct tm_simple *tm)
{
  // move from 1970 to 1 as epoch, to be able to use division with positive values
  seconds       = seconds + (719528ull - 366ull)*86400ull;
  tm->sec       = Math::div64(seconds, 60);
  tm->min       = Math::div64(seconds, 60);
  tm->hour      = Math::div64(seconds, 24);
  timevalue days= seconds++;
  tm->wday      = Math::div64(seconds, 7);
  unsigned years400 = moddiv(days, 4*36524+1);
  unsigned years100 = moddiv(days, 36524);
  // overflow on a 400year boundary?
  if (years100 == 4) years100--, days += 36524;

  unsigned years4 = moddiv(days, 1461);
  unsigned years  = moddiv(days, 365);
  // overflow on the 4year boundary?
  if (years == 4)  years -= 1, days += 365;



  // move back to our timebase
  tm->year = ((((years400 << 2) + years100)*25 + years4) << 2) + years + 1;
  tm->yday  = days + 1;

  // get month,day from day_of_year
  static unsigned leap[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366};
  static unsigned noleap[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
  unsigned *l = is_leap(tm->year) ? leap : noleap;

  // heuristic to (under-)estimate the month
  unsigned m = static_cast<unsigned>(days) / 31;
  days -= l[m];
  // did we made it?
  if (days >= (l[m+1] - l[m]))
    {
      days -= l[m+1] - l[m];
      m += 1;
    }
  tm->mon  = m + 1;
  tm->mday = days + 1;
}
