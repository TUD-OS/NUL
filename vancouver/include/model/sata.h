/** @file
 * Shared SATA definitions.
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

class FisReceiver
{
 protected:
  FisReceiver *_peer;
 public:
 FisReceiver() : _peer(0) {}
  virtual void receive_fis(unsigned fislen, unsigned *fis) = 0;
  void set_peer(FisReceiver *peer) { _peer = peer; }
};
