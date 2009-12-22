/*
 * A semaphore implementation for NOVA.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of Vancouver.
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

#include "vmm/cpu.h"
#include "driver/logging.h"


/**
 * A kernel semaphore optimized for consumer/producer.
 */
class KernelSemaphore
{
  unsigned _sm;
public:
  KernelSemaphore(unsigned cap_sm) : _sm(cap_sm) {}
  void down()
  {
    unsigned res = semdown(_sm);
    if (res) Logging::panic("notify failed in %s with %x", __PRETTY_FUNCTION__, res);
  } 


  void up()
  { 
    unsigned res = semup(_sm);
    if (res) Logging::panic("notify failed in %s with %x", __PRETTY_FUNCTION__, res);
  }


  unsigned sm() { return _sm; };
};



/**
 * A user semaphore optimized for the case where we do not block.
 */
class Semaphore
{
  KernelSemaphore _sem;
  long *_count;
public:
  Semaphore(long *count = 0, unsigned cap_sm = 0) : _sem(cap_sm), _count(count) { };
  
  void down() {  if (Cpu::atomic_xadd(_count, -1) <= 0)  _sem.down(); }
  void up()   {  if (Cpu::atomic_xadd(_count, +1) <  0)  _sem.up();   }
  unsigned sm() {  return  _sem.sm();  }
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
