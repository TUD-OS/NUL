/*
 * A semaphore implementation for NOVA.
 *
 * Copyright (C) 2008, Bernhard Kauer <kauer@tudos.org>
 *
 * This file is part of Vancouver.nova.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
#pragma once

#include "driver/logging.h"

#include <nova.h>

/**
 * A semaphore.
 */
class Semaphore
{

  long *_count;
  Nova::Cap_idx _sm;
public:
  Semaphore(long *count = 0, Nova::Cap_idx cap_sm = 0) :_count(count), _sm(cap_sm) { };
  

  void down()
  {
    if (Cpu::atomic_xadd(_count, -1) > 0)  return;
    unsigned res = Nova::semdown(_sm);
    if (res) Logging::panic("notify failed in semaphore->down() with %x", res);
  }
  

  void up()
  { 
    if (Cpu::atomic_xadd(_count, +1) >= 0)  return;
    unsigned res = Nova::semup(_sm);
    if (res) Logging::panic("notify failed in semaphore->up() with %x", res);
  }
  Nova::Cap_idx sm() { return _sm; };
};


/**
 * A Guard object for simplicity.
 */
class SemaphoreGuard
{
  Semaphore &_sem;
public:
  SemaphoreGuard(Semaphore &sem) : _sem(sem) {  _sem.down(); }
  ~SemaphoreGuard()                          {  _sem.up();   }
};
