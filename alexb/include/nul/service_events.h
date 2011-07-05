/*
 * Client part of the event protocol.
 *
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

#include <nul/types.h>
#include <service/string.h>
#include <nul/parent.h>

/**
 */
struct EventsProtocol : public GenericProtocol {

  enum {
    TYPE_GET_EVENTS_INFO = ParentProtocol::TYPE_GENERIC_END,
  };
  enum {
    EVENT_REBOOT = 0xbbbb,
    EVENT_UNSERVED_IOACCESS = 0xbbc0,
  };

  unsigned send_event(Utcb &utcb, unsigned id, unsigned data_len = 0, const void * data = 0) {
    return call_server(init_frame(utcb, TYPE_GET_EVENTS_INFO) << id << data_len
           << Utcb::String(reinterpret_cast<char const *>(data), data_len), true);
  }

  EventsProtocol(unsigned cap_base, unsigned instance=0) : GenericProtocol("events", instance, cap_base, true) {}
};

// EOF
