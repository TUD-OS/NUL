/** @file
 * Client part of the log protocol.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011, Alexander Boettcher <boettcher@tudos.org>
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
#pragma once

#include "parent.h"
#include "host/dma.h"


/**
 * Client part of the disk protocol.
 * Missing: register shared memory producer/consumer.
 */
struct DiscProtocol : public GenericProtocol {
  enum {
    TYPE_GET_PARAMS = ParentProtocol::TYPE_GENERIC_END,
    TYPE_READ,
    TYPE_WRITE,
    TYPE_FLUSH_CACHE,
    TYPE_GET_COMPLETION,
  };

  unsigned get_params(Utcb &utcb, DiskParameter *params) {
    unsigned res;
    if (!(res = call_server(init_frame(utcb, TYPE_GET_PARAMS), false)))
      if (utcb >> *params)  res = EPROTO;
    utcb.drop_frame();
    return res;
  }


  unsigned read_write(Utcb &utcb, bool read, unsigned long usertag, unsigned long long sector,
		unsigned long physoffset, unsigned long physsize,
		unsigned dmacount, DmaDescriptor *dma)
  {
    init_frame(utcb, read ? TYPE_READ : TYPE_WRITE) << usertag << sector << physoffset << physsize << dmacount;
    for (unsigned i=0; i < dmacount; i++)  utcb << dma[i];
    return call_server(init_frame(utcb, TYPE_GET_PARAMS), true);
  }



  unsigned flush_cache(Utcb &utcb) {
    return call_server(init_frame(utcb, TYPE_FLUSH_CACHE), true);
  }


  unsigned get_completion(Utcb &utcb, unsigned &tag, unsigned &status) {
    unsigned res;
    if (!(res = call_server(init_frame(utcb, TYPE_GET_COMPLETION), false)))
      if (utcb >> tag || utcb >> status)  res = EPROTO;
    utcb.drop_frame();
    return res;
  }

  DiscProtocol(unsigned cap_base, unsigned disknr) : GenericProtocol("disk", disknr, cap_base, true) {}
};
