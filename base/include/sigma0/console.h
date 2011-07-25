/** @file
 * Programm console helper.
 *
 * Copyright (C) 2008-2010, Bernhard Kauer <bk@vmmon.org>
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

#include "nul/message.h"
#include "host/screen.h"
#include "sigma0/sigma0.h"
#include "nul/parent.h"
#include "nul/baseprogram.h"
#include "nul/service_log.h"

class MultiEntranceLock {
  void *_lock_owner;
  int   _nesting_level;
 public:
  Semaphore * sem;
  MultiEntranceLock() : _lock_owner(0), _nesting_level(0), sem(0) {}

  void lock(void *myself) {
    if (_lock_owner != myself) {
      if (sem) sem->down();
      _lock_owner = myself;
    }
    _nesting_level++;
  }

  void unlock(void *myself) {
    assert(_lock_owner == myself);
    if (!--_nesting_level) {
      _lock_owner = 0;
      if (sem) sem->up();
    }
  }

  int nesting_level() { return _nesting_level - 1; }
};

/**
 * A helper class that implements a vga console and forwards message to sigma0.
 */
class ProgramConsole
{
 protected:
  struct console_data {
    VgaRegs *regs;
    unsigned short *screen_address;
    unsigned index;
    MultiEntranceLock lock;
    char buffer[200];
    LogProtocol *log;
  };

  static void putc(void *data, int value)
  {
    struct console_data *d = reinterpret_cast<struct console_data *>(data);
    if (value == -1) d->lock.lock(BaseProgram::myutcb());
    if (value == -2) d->lock.unlock(BaseProgram::myutcb());
    if (value < 0) return;
    if (d->screen_address)  Screen::vga_putc(0xf00 | value, d->screen_address, d->regs->cursor_pos);
    if (value == '\n' || d->index == sizeof(d->buffer) - 1)
      {
        d->buffer[d->index] = 0;
        if (d->log && !d->lock.nesting_level())
          d->log->log(*BaseProgram::myutcb(), d->buffer);
        d->index = 0;
        if (value != '\n')
        {
          d->buffer[d->index++] = '|';
          d->buffer[d->index++] = ' ';
          d->buffer[d->index++] = value;
        }
      }
    else
      d->buffer[d->index++] = value;
  }


  VgaRegs             _vga_regs;
  struct console_data _console_data;
  char                _vga_console[0x1000];
  void console_init(const char *name, Semaphore * sem)
  {
    assert(sem);
    _console_data.screen_address = reinterpret_cast<unsigned short *>(_vga_console);
    _console_data.regs      = &_vga_regs;
    _console_data.lock.sem = sem;
    sem->up();
    Logging::init(putc, &_console_data);
    _vga_regs.cursor_pos = 24*80*2;
    _vga_regs.offset = 0;
    _vga_regs.cursor_style = 0x2000;


    MessageConsole msg(name, _vga_console, sizeof(_vga_console), &_vga_regs);
    Sigma0Base::console(msg);
  }
};
