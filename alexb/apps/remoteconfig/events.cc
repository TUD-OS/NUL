/*
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

#include "events.h"
#include <nul/service_events.h>

  void EventService::check_clients(Utcb &utcb) {
    ClientDataStorage<ClientData, EventService>::Guard guard_c(&_storage, utcb, this);
    ClientData * data = _storage.get_invalid_client(utcb, this);
    while (data) {
      Logging::printf("ad: found dead client - freeing datastructure\n");
      _storage.free_client_data(utcb, data, this);
      data = _storage.get_invalid_client(utcb, this, data);
    }
  }

  void NORETURN EventService::portal_pf(EventService *tls, Utcb *utcb)
  {
    Logging::printf("daemon: worker thread died - pagefault %x/%x for %llx err %llx at %x\n",
       utcb->head.untyped, utcb->head.typed, utcb->qual[1], utcb->qual[0], utcb->eip);
    while(1) {}
  }

  unsigned EventService::portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
    {
      unsigned op, res;
      check1(EPROTO, input.get_word(op));

      switch (op) {
      case EventsProtocol::TYPE_GET_EVENTS_INFO:
      {
        unsigned eventid, extra_len, extra_len2;
        char * extra = 0;

        check1(EPROTO, input.get_word(eventid));
        check1(EPROTO, input.get_word(extra_len));
        if (extra_len > 0) extra = input.get_string(extra_len2);

        long guid = 0xaffe;
        ClientData *data = 0;
        ClientDataStorage<ClientData, EventService>::Guard guard_c(&_storage, utcb, this);
        if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

        res = ParentProtocol::get_quota(utcb, data->pseudonym, "guid", 0, &guid);
 
        bool bres = server->push_event(guid, eventid, extra_len, extra);
        if (enable_verbose) Logging::printf("        - got event from guid=%ld eventid=%x res=0x%x forwarded=%s\n", guid, eventid, res, bres ? "yes" : "no");
        return bres ? ENONE : EABORT;
      }
      case ParentProtocol::TYPE_OPEN:
      {
        unsigned idx = input.received_cap();

        if (enable_verbose && !idx) Logging::printf("  open - invalid cap recevied\n");
        if (!idx) return EPROTO;

        ClientData *data = 0;
        res = _storage.alloc_client_data(utcb, data, idx, this);
        if (enable_verbose && res) Logging::printf("  alloc_client - res %x\n", res);
        if (res == ERESOURCE) { check_clients(utcb); return ERETRY; } //force garbage collection run
        else if (res) return res;
        if (*flag_revoke) { check_clients(utcb); *flag_revoke = 0; }

        free_cap = false;
        if (enable_verbose) Logging::printf("**** created event client 0x%x 0x%x\n", data->pseudonym, data->get_identity());
        utcb << Utcb::TypedMapCap(data->get_identity());
        return res;
      }
      case ParentProtocol::TYPE_CLOSE:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, EventService>::Guard guard_c(&_storage, utcb, this);
        check1(res, res = _storage.get_client_data(utcb, data, input.identity()));
        return _storage.free_client_data(utcb, data, this);
      }
      default:
        Logging::printf("unknown proto\n");
        return EPROTO;
      }
    }
