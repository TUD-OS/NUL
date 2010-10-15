/*
 * Parent protocol implementation in sigma0.
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

/**
 * Missing: kill a client, mem+cap quota support
 */
struct ClientData : public GenericClientData {
  char          * name;
  unsigned        len;
  static unsigned get_quota(Utcb &utcb, unsigned parent_cap, const char *name, long value_in, long *value_out=0) {
    Utcb::Frame input = utcb.get_nested_frame();
    Logging::printf("get quota for '%s' amount %lx from %x for %x by %x\n", name, value_in, parent_cap, input.identity(1), input.identity(0));
    if (!strcmp(name, "mem") || !strcmp(name, "cap")) return ENONE;
    if (!strcmp(name, "guid")) {
      char *cmdline = get_module_cmdline(input);
      if (cmdline && strstr(cmdline, "quota::guid")) {
	*value_out = get_client_number(parent_cap);
	Logging::printf("send clientid %lx from %x\n", *value_out, parent_cap);
	return ENONE;
      }
    }
    return ERESOURCE;
  }
  void session_close(Utcb &utcb) {}
};
struct ServerData : public ClientData {
  unsigned        cpu;
  unsigned        pt;
};
ClientDataStorage<ServerData> _server;
ClientDataStorage<ClientData> _client;

static unsigned get_client_number(unsigned cap) {
  // we handle ourself as client 0!
  if (cap >= CLIENT_PT_OFFSET) cap -= CLIENT_PT_OFFSET;
  if ((cap % (1 << CLIENT_PT_SHIFT)) != ParentProtocol::CAP_PARENT_ID) return ~0;
  return cap >> CLIENT_PT_SHIFT;
}

static char * get_module_cmdline(Utcb::Frame &input) {
  unsigned clientnr = get_client_number(input.identity(0));
  if (clientnr >= MAXMODULES) return 0;
  return sigma0->_modinfo[clientnr].cmdline;
}


unsigned free_service(Utcb &utcb, ServerData *sdata) {
  dealloc_cap(sdata->pt);
  delete sdata->name;
  ServerData::get_quota(utcb, sdata->parent_cap, "cap", -1);
  ServerData::get_quota(utcb, sdata->parent_cap, "mem", -sdata->len);
  return _server.free_client_data(utcb, sdata);
}


unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap) {
  // we lock to protect our datastructures and the malloc implementation
  SemaphoreGuard l(_lock);

  unsigned res;
  ServerData *sdata;
  ClientData *cdata;
  unsigned op;
  if (input.get_word(op)) return EPROTO;
  Logging::printf("parent request words %x type %x id %x+%x\n", input.untyped(), op, input.identity(), input.identity(1));
  switch (op) {
  case ParentProtocol::TYPE_OPEN:
    {
      unsigned instance;
      const char *request;
      unsigned request_len;
      if (input.get_word(instance) || !(request = input.get_zero_string(request_len))) return EPROTO;

      /**
       * Parse the cmdline for "name::" prefixes and check whether the
       * postfix matches the requested name.
       */
      char * cmdline = get_module_cmdline(input);
      if (!cmdline) return EPROTO;
      cmdline = strstr(cmdline, "name::");
      while (cmdline) {
	cmdline += 6;
	unsigned namelen = strcspn(cmdline, " \t\r\n\f");
	if ((request_len > namelen) || (0 != memcmp(cmdline + namelen - request_len, request, request_len))) {
	  cmdline = strstr(cmdline + namelen, "name::");
	  continue;
	}
	if (instance--) continue;

	// check whether such a session is already known from our client
	for (ClientData * c = _client.next(); c; c = _client.next(c))
	  if (c->name == cmdline && c->parent_cap == input.identity())
	    return EEXISTS;

	check1(res, res = _client.alloc_client_data(utcb, cdata, input.identity()));
	cdata->name = cmdline;
	cdata->len  = namelen;
	utcb << Utcb::TypedMapCap(cdata->identity);
	Logging::printf("\treturn cap %x to client %x\n", cdata->identity, input.identity());
	return ENONE;
      }
      // we do not have the permissions
      return EPERM;
    }

  case ParentProtocol::TYPE_CLOSE:
    if ((res = _client.get_client_data(utcb, cdata, input.identity(1)))) return res;
    Logging::printf("\tclose session for %x for %x\n", cdata->identity, cdata->parent_cap);
    return _client.free_client_data(utcb, cdata);

  case ParentProtocol::TYPE_REQUEST:
    {
      if ((res = _client.get_client_data(utcb, cdata, input.identity(1)))) return res;
      Logging::printf("\tfound session cap %x for client %x %.*s\n", cdata->identity, cdata->parent_cap, cdata->len, cdata->name);
      for (sdata = _server.next(); sdata; sdata = _server.next(sdata))
	if (sdata->cpu == Cpu::cpunr() && cdata->len == sdata->len-1 && !memcmp(cdata->name, sdata->name, cdata->len)) {
	  Logging::printf("\tfound service cap %x\n", sdata->pt);

	  // check that the portal still exists
	  unsigned crdout;
	  if (nova_syscall(NOVA_LOOKUP, Crd(sdata->pt, 0, DESC_CAP_ALL).value(), 0, 0, 0, &crdout) || !(crdout & DESC_RIGHTS_ALL)) {
	    free_service(utcb, sdata);
	    return ERETRY;
	  }
	  Logging::printf("mapping portal %x back\n", sdata->pt);
	  utcb << Utcb::TypedMapCap(sdata->pt);
	  return ENONE;
	}
      return ERETRY;
    }

  case ParentProtocol::TYPE_REGISTER:
    {
      unsigned cpu;
      char *request;
      unsigned request_len;
      if (input.get_word(cpu) || !(request = input.get_zero_string(request_len))) return EPROTO;

      // search for an allowed namespace
      char * cmdline = get_module_cmdline(input);
      if (!cmdline) return EPROTO;
      Logging::printf("\tregister client %x cmdline '%.10s'\n", input.identity(), cmdline);
      cmdline = strstr(cmdline, "namespace::");
      if (!cmdline) return EPERM;
      cmdline += 11;
      unsigned namespace_len = strcspn(cmdline, " \t");

      QuotaGuard<ServerData> guard1(utcb, input.identity(), "mem", request_len + namespace_len + 1);
      QuotaGuard<ServerData> guard2(utcb, input.identity(), "cap", 1, &guard1);
      check1(res, res = guard2.status());
      check1(res, (res = _server.alloc_client_data(utcb, sdata, input.identity())));
      guard2.commit();

      sdata->len = namespace_len + request_len + 1;
      sdata->name = new char[sdata->len];
      memcpy(sdata->name, cmdline, namespace_len);
      memcpy(sdata->name + namespace_len, request, request_len);
      sdata->name[sdata->len - 1] = 0;
      sdata->cpu  = cpu;
      sdata->pt   = input.received_cap();

      for (ServerData * s2 = _server.next(); s2; s2 = _server.next(s2))
	if (s2->len == sdata->len && !memcmp(sdata->name, s2->name, sdata->len) && sdata->cpu == s2->cpu && sdata->pt != s2->pt) {
	  free_service(utcb, sdata);
	  return EEXISTS;
	}

      // wakeup clients that wait for us
      for (ClientData * c = _client.next(); c; c = _client.next(c))
	if (c->len == sdata->len-1 && !memcmp(c->name, sdata->name, c->len)) {
	  Logging::printf("\tnotify client %x\n", c->parent_cap);
	  nova_semup(c->identity);
	}

      utcb << Utcb::TypedMapCap(sdata->identity);
      free_cap = false;
      return ENONE;
    }

  case ParentProtocol::TYPE_UNREGISTER:
    if ((res = _server.get_client_data(utcb, sdata, input.identity(1)))) return res;
    Logging::printf("\tunregister %s cpu %x\n", sdata->name, sdata->cpu);
    return free_service(utcb, sdata);

  case ParentProtocol::TYPE_GET_QUOTA:
    {
      if ((res = _client.get_client_data(utcb, cdata, input.identity(1)))) return res;
      long invalue;
      char *request;
      unsigned request_len;
      if (input.get_word(invalue) || !(request = input.get_zero_string(request_len))) return EPROTO;

      long outvalue = invalue;
      res = ClientData::get_quota(utcb, cdata->parent_cap, request,  invalue, &outvalue);
      utcb << outvalue;
      return res;
    }
  default:
    return EPROTO;
  }
}
