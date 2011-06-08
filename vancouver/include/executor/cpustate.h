/** @file
 * Generic cpu state.
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
#include "sys/utcb.h"
#include "service/cpu.h"

/**
 * A generic cpu state class.
 */
class CpuState : public Utcb
{
 public:
  unsigned cpl()   { return (ss.ar >> 5) & 3; }
  unsigned iopl()  { return (efl >> 12) & 3; }
  unsigned pm()    { return cr0 & 0x1; }
  unsigned pg()    { return cr0 & 0x80000000; }
  unsigned v86()   { return cr0 & 0x1 && efl & (1 << 17); }
  void edx_eax(unsigned long long value)
  {
    eax = value;
    edx = value >> 32;
  };

  unsigned long long edx_eax() {  return union64(edx, eax); };
};
