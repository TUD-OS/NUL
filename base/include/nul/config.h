/** @file
 * Global Config.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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

struct Config {
  enum {
    NUL_VERSION        = 0x00000003204c554eULL,
    CAP_RESERVED_ORDER = 10, //reserved cap region where the parent can put caps for the client
    MAX_CLIENTS_ORDER  = 6,
    PHYS_ADDR_SIZE     = 40,
    MAX_CPUS           = 32,
  };
};
