/**
 * @file 
 * Pong-part of cross-PD ping-pong benchmark
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

class Pong  : public WvProgram
{
  static void portal_func(unsigned long mtr) { }

public:
  void wvrun(Utcb *utcb, Hip *hip)
  {
    unsigned service_cap = alloc_cap();
    cap_sel ec;
    WVPASS(ec = create_ec4pt(this, utcb->head.nul_cpunr, 0));
    WVSHOWHEX(ec);
    cap_sel pt = alloc_cap();
    WVNOVA(nova_create_pt(pt, ec, reinterpret_cast<unsigned long>(portal_func), 0));
    WVNUL(ParentProtocol::register_service(*utcb, "/pong", utcb->head.nul_cpunr, pt, service_cap));
  }
};

ASMFUNCS(Pong, WvTest)
