/*
 * Config service interface.
 *
 * Copyright (C) 2010, Alexander Boettcher <boettcher@tudos.org>
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

#include "nul/motherboard.h"
#include "nul/generic_service.h"
#include "nul/service_config.h"
#include "nul/service_fs.h"
#include "nul/region.h"
#include "nul/capalloc.h"

class Service_config : public CapAllocator<Service_config> {
  Motherboard &mb;

public:
  Service_config(Motherboard &_mb, unsigned long capstart, unsigned long cap_order)
    : CapAllocator<Service_config> (capstart, capstart, cap_order), mb(_mb) {}

  inline unsigned alloc_crd() { return alloc_cap() << Utcb::MINSHIFT | DESC_TYPE_CAP; }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap) {
    unsigned op;

    check1(EPROTO, input.get_word(op));

    switch (op) {
    case ParentProtocol::TYPE_OPEN:
    case ParentProtocol::TYPE_CLOSE:
      return ENONE;
    case ConfigProtocol::TYPE_START_CONFIG:
      {
        unsigned len = 0;
        char *_config = input.get_zero_string(len);
        check1(EPROTO, !_config);

        char *config = new char[len + 1];
        check1(ERESOURCE, !config);
        memcpy(config, _config, len + 1);

        MessageConsole msg(MessageConsole::TYPE_START, ~0);
        msg.cmdline = config;
        if (mb.bus_console.send(msg)) {
          //XXX utcb.head.mtr is modified by sigma0.cc (by map_self), first untyped word is error code
          utcb.head.untyped = 1;
          return ENONE;
        } else {
          //XXX utcb.head.mtr is modified by sigma0.cc (by map_self), first untyped word is error code
          utcb.head.untyped = 1;
          delete [] config;
          return EPROTO;
        }
      }
    default:
      return EPROTO;
    }
  }

};

PARAM(service_config,
      unsigned cap_region = alloc_cap_region(1 << 12, 12);
      Service_config *service_config = new Service_config(mb, cap_region, 12);

      MessageHostOp msg(service_config, "/config", reinterpret_cast<unsigned long>(StaticPortalFunc<Service_config>::portal_func));
      if (!mb.bus_hostop.send(msg))
        Logging::panic("starting of config service failed");
      //Logging::printf("start service - config\n");
      ,
      "service_config - managing a config, e.g. start, stop");

