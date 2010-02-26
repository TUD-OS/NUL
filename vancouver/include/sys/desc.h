#pragma once
#include "service/assert.h"

enum
  {
    MTD_GPR_ACDB        = 1ul << 0,
    MTD_GPR_BSD         = 1ul << 1,
    MTD_RSP             = 1ul << 2,
    MTD_RIP_LEN         = 1ul << 3,
    MTD_RFLAGS          = 1ul << 4,
    MTD_DS_ES           = 1ul << 5,
    MTD_FS_GS           = 1ul << 6,
    MTD_CS_SS           = 1ul << 7,
    MTD_TR              = 1ul << 8,
    MTD_LDTR            = 1ul << 9,
    MTD_GDTR            = 1ul << 10,
    MTD_IDTR            = 1ul << 11,
    MTD_CR              = 1ul << 12,
    MTD_DR              = 1ul << 13,
    MTD_SYSENTER        = 1ul << 14,
    MTD_QUAL            = 1ul << 15,
    MTD_CTRL            = 1ul << 16,
    MTD_INJ             = 1ul << 17,
    MTD_STATE           = 1ul << 18,
    MTD_TSC             = 1ul << 19,
    MTD_IRQ             = MTD_RFLAGS | MTD_STATE | MTD_INJ,
    MTD_ALL             = (~0U >> 13) & ~MTD_CTRL,
  };

enum
  {
    INJ_IRQWIN = 0x1000,
  };


class Desc
{
protected:
  unsigned _value;
  Desc(unsigned v) : _value(v) {}
public:
  unsigned value() { return _value; }
};

/**
 * A capability range descriptor;
 */
class Crd : public Desc
{
public:
  unsigned order() { return ((_value >> 7) & 0x1f); }
  unsigned size() { return 1 << (order() + 12); }
  Crd(unsigned offset, unsigned order, unsigned type = 3) : Desc((offset << 12) | (order << 7) | type) { assert(!(offset >> 22)); assert(!(order >> 7)); }
  Crd(unsigned v) : Desc(v) {}
};

/**
 * A message transfer descriptor.
 */
class Mtd : public Desc
{
public:
  Mtd() : Desc(0) {}
  Mtd(unsigned _untyped, unsigned _typed) : Desc((_typed << 23) | _untyped) { /* assert(!(_typed >> 9)); assert(!(_untyped >> 23)); */ }
  unsigned typed() { return (_value  >> 23); }
  unsigned untyped() { return _value & ~0xff800000; }
  void add (unsigned v) { _value |= v; };
  void del (unsigned v) { _value &= ~v; };
};


/**
 * A quantum+period descriptor.
 */
class Qpd : public Desc
{
public:
  Qpd(unsigned prio, unsigned quantum) : Desc((quantum << 12) | prio) { assert(!(prio >> 8)); assert(!(prio >> 20)); }
};

