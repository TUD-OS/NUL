/**
 * @file 
 * UTCB bounds check etc.
 *
 * Copyright (C) 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <wvprogram.h>

template<unsigned S>
class Array {
  unsigned x[S];
public:
  operator Array&() { return *this; }  
};

class UtcbBounds : public WvProgram
{
public:
  void wvrun(Utcb *utcb, Hip *hip)
  {
    static Utcb u;
    unsigned size;
    WVSHOW(static_cast<int>(Utcb::STACK_START));
    WVSHOW(static_cast<int>(Utcb::MAX_DATA_WORDS));
    WVSHOW(static_cast<int>(Utcb::MAX_FRAME_WORDS));
    u.reset();
    WVSHOW(size = u.frame_words());
    WVPASSEQ((u << 1).frame_words(), size + 1);
    WVPASSEQ((u << Utcb::TypedMapCap(0)).frame_words(), size + 3);

    WV(u.reset());
    WVPASS((u << *new Array<Utcb::MAX_FRAME_WORDS - 5>()).validate_recv_bounds());
    WVFAIL((u << *new Array<Utcb::MAX_FRAME_WORDS - 4>()).validate_recv_bounds());

    WV(u.reset());
    WV(for (int i=0; i<(Utcb::MAX_FRAME_WORDS-5)/2; i++) u << Utcb::TypedMapCap(0));
    WVPASS(u.validate_recv_bounds());
    WVFAIL((u << Utcb::TypedMapCap(0)).validate_recv_bounds());    

    WV(u.reset());
    WV((u << *new Array<(Utcb::MAX_FRAME_WORDS - 5)/2+1>()));
    WV(for (int i=0; i<(Utcb::MAX_FRAME_WORDS-5)/2/2; i++) u << Utcb::TypedMapCap(0));
    WVPASS(u.validate_recv_bounds());
    WVFAIL((u << 0).validate_recv_bounds());
  }
};

ASMFUNCS(UtcbBounds, WvTest)
