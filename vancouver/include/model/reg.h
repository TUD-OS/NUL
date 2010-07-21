/**
 * Generic hardware register definition.
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
#define DEFINE_REG(NAME, OFFSET, VALUE, MASK) private: unsigned NAME; public: static const unsigned NAME##_offset = OFFSET; static const unsigned NAME##_mask   = MASK; static const unsigned NAME##_reset  = VALUE;
#define REG_RO(NAME, OFFSET, VALUE) REG(NAME, OFFSET, static const unsigned NAME = VALUE;, value = VALUE; , break; , )
#define REG_RW(NAME, OFFSET, VALUE, MASK, WRITE_CALLBACK) REG(NAME, OFFSET, DEFINE_REG(NAME, OFFSET, VALUE, MASK) , value = NAME; , if (!MASK) return false; if (strict && value & ~MASK) return false; NAME = (NAME & ~MASK) | (value & MASK); WRITE_CALLBACK; , NAME=VALUE;)
#define REG_WR(NAME, OFFSET, VALUE, MASK, RW1S, RW1C, WRITE_CALLBACK) REG(NAME, OFFSET, DEFINE_REG(NAME, OFFSET, VALUE, MASK), value = NAME; ,  if (!MASK) return false; unsigned oldvalue = NAME; value = value & ~RW1S | ( value | oldvalue) & RW1S; value = value & ~RW1C | (~value & oldvalue) & RW1C; NAME = (NAME & ~MASK) | (value & MASK); WRITE_CALLBACK; , NAME = VALUE;)
#define REGSET(NAME, ...) private: __VA_ARGS__
#define REG(NAME, OFFSET, MEMBER, READ, WRITE, RESET) MEMBER
#include REGBASE
#undef  REG
#undef  REGSET
#define REGSET(NAME, ...)  bool NAME##_read(unsigned offset, unsigned &value) { switch (offset) { __VA_ARGS__ default: break; } return false; }
#define REG(NAME, OFFSET, MEMBER, READ, WRITE, RESET) case OFFSET:  { READ }; return true;
#include REGBASE
#undef  REG
#undef  REGSET
#define REGSET(NAME, ...)  bool NAME##_write(unsigned offset, unsigned value, bool strict=false) { switch (offset) { __VA_ARGS__ default: break; } return 0; }
#define REG(NAME, OFFSET, MEMBER, READ, WRITE, RESET) case OFFSET:  { WRITE }; return true;
#include REGBASE
#undef  REG
#undef  REGSET
#define REGSET(NAME, ...)  void NAME##_reset() { __VA_ARGS__ }; private:
#define REG(NAME, OFFSET, MEMBER, READ, WRITE, RESET) RESET
#include REGBASE
#undef  REG
#undef  REGSET
#undef  REG_WR
#undef  REG_RW
#undef  REG_RO
#undef  DEFINE_REG
#undef REGBASE
