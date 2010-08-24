/*
 * (C) 2010 Alexander Boettcher
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <sigma0/sigma0.h>
#include <service/string.h> //memcpy

class NameSpaceBase {
  typedef unsigned long mword;

  public:
    class MessageNS {
      public:
        char name [128];
    };

    enum {
      PORT_BASE = 0x60,
      REQUEST_NS_REGISTER = 0x2001,
      REQUEST_NS_RESOLVE
    };

  static bool announce (Utcb *utcb, MessageNS &msg, unsigned pt_base, unsigned pt_order) {
    const unsigned words = (sizeof(msg) +  sizeof(mword) - 1) / sizeof(mword);
    unsigned cpu = Cpu::cpunr();

    utcb->msg[0]   = REQUEST_NS_REGISTER;
    utcb->head.mtr = Mtd(1 + words, 0);

    memcpy(utcb->msg + 1, &msg,  words*sizeof(mword));

    Sigma0Base::add_mappings(utcb, false, pt_base << Utcb::MINSHIFT,
                             1 << pt_order << Utcb::MINSHIFT, 0, DESC_CAP_ALL);

    if (nova_call(PORT_BASE + cpu, utcb->head.mtr))
      return false;

    return (!utcb->msg[0]);
  }

  static bool resolve (Utcb * utcb, MessageNS &msg, unsigned cap, unsigned order) {
    unsigned cpu = Cpu::cpunr();
    const unsigned words = (sizeof(msg) +  sizeof(mword) - 1) / sizeof(mword);

    utcb->head.crd = Crd(cap, order, DESC_CAP_ALL).value();
    utcb->msg[0] = REQUEST_NS_RESOLVE;
    memcpy(utcb->msg + 1, &msg,  words*sizeof(mword));

    if (nova_call(PORT_BASE + cpu, Mtd(1 + words, 0)))
      return false;

    if (utcb->msg[0])
      return false;

    if (utcb->head.mtr.typed() == 0)
      return false;

    utcb->head.crd = Crd(0,0,0).value();

    return true;
  }

};
