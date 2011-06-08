/** @file
 * Generic MP LIFO implementation.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "service/cpu.h"

/**
 * Generic MP-save LIFO implementation.
 */
template <typename T>
class AtomicLifo {
  T *_head;
public:
  AtomicLifo() : _head(0) {}

  void enqueue(T volatile *value) {
    T *old;
    do {
      old = _head;
      value->lifo_next = old;
    } while (Cpu::cmpxchg4b(reinterpret_cast<unsigned *>(&_head), reinterpret_cast<unsigned>(old), reinterpret_cast<unsigned>(value)) != reinterpret_cast<unsigned>(old));
  }

  T *dequeue_all() { return Cpu::xchg(&_head, static_cast<T*>(NULL)); }
  T *head() { return _head; }
};
