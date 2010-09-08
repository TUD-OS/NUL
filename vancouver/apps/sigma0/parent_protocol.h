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
 * Missing: kill a client, quota support
 */


struct ClientData : public GenericClientData {
  char          * name;
  unsigned        len;
  static unsigned get_quota(Utcb *utcb, unsigned parent_cap, const char *name, long value_in, long *value_out=0) { return ENONE; }
  void session_close(Utcb *utcb) {}
};
struct ServerData : public ClientData {
  unsigned        cpu;
  unsigned        pt;
};
ClientDataStorage<ServerData> _server;
ClientDataStorage<ClientData> _client;


unsigned free_service(Utcb *utcb, ServerData *sdata) {
  dealloc_cap(sdata->pt);
  delete sdata->name;
  ServerData::get_quota(utcb, sdata->parent_cap, "cap", -1);
  ServerData::get_quota(utcb, sdata->parent_cap, "mem", -sdata->len);
  return _server.free_client_data(utcb, sdata);
}


unsigned handle_parent(Utcb *utcb, unsigned clientnr, bool &free_cap) {
  unsigned res;
  ServerData *sdata;
  ClientData *cdata;
  Logging::printf("parent request client %x mtr %x/%x type %x \n", clientnr, utcb->head.mtr.untyped(), utcb->head.mtr.typed(), utcb->msg[0]);
  if (utcb->head.mtr.untyped() < 1) return EPROTO;
  switch (utcb->msg[0]) {
  case ParentProtocol::TYPE_OPEN:
    {
      if (utcb->head.mtr.untyped() < 2) return EPROTO;
      char *request = reinterpret_cast<char *>(utcb->msg+1);
      unsigned request_len = sizeof(unsigned)*(utcb->head.mtr.untyped()-1);
      request[request_len-1] = 0;
      request_len = strnlen(request, request_len);

      /**
       * Parse the cmdline for "name::" prefixes and check whether the
       * postfix matches the requested name.
       */
      char * cmdline = strstr(_modinfo[clientnr].cmdline, "name::");
      while (cmdline) {
	cmdline += 6;
	unsigned namelen = strcspn(cmdline, " \t\r\n\f");
	if ((request_len > namelen) || (0 != memcmp(cmdline + namelen - request_len, request, request_len))) {
	  cmdline = strstr(cmdline + namelen, "name::");
	  continue;
	}

	// check whether such a session is already known from our client
	for (ClientData * c = _client.next(); c; c = _client.next(c))
	  if (c->name == cmdline && c->parent_cap == clientnr)
	    return EEXISTS;

	check1(res, res = _client.alloc_client_data(utcb, cdata, clientnr));
	cdata->name = cmdline;
	cdata->len  = namelen;
	ParentProtocol::init_utcb(utcb, 1, 0, utcb->head.crd >> Utcb::MINSHIFT, cdata->identity);
	Logging::printf("return cap %x to client %x\n", cdata->identity, clientnr);
	return ENONE;
      }
      // we do not have the permissions
      return EPERM;
    }

  case ParentProtocol::TYPE_CLOSE:
    check1(res, res = _client.get_client_data(utcb, cdata));
    Logging::printf("close session for %x for %x\n", cdata->identity, cdata->parent_cap);
    return _client.free_client_data(utcb, cdata);

  case ParentProtocol::TYPE_REQUEST:
    {
      check1(res, res = _client.get_client_data(utcb, cdata));
      Logging::printf("found session cap %x for client %x %.*s\n", cdata->identity, cdata->parent_cap, cdata->len, cdata->name);
      for (sdata = _server.next(); sdata; sdata = _server.next(sdata))
	if (sdata->cpu == Cpu::cpunr() && cdata->len == sdata->len-1 && !memcmp(cdata->name, sdata->name, cdata->len)) {
	  Logging::printf("found service cap %x\n", sdata->pt);

	  // check that the portal still exists
	  unsigned crdout;
	  if (nova_syscall(NOVA_LOOKUP, Crd(sdata->pt, 0, DESC_CAP_ALL).value(), 0, 0, 0, &crdout) || !(crdout & DESC_RIGHTS_ALL)) {
	    free_service(utcb, sdata);
	    return ERETRY;
	  }
	  ParentProtocol::init_utcb(utcb, 1, 0, utcb->head.crd >> Utcb::MINSHIFT, sdata->pt);
	  return ENONE;
	}
      return ERETRY;
    }

  case ParentProtocol::TYPE_REGISTER:
    {
      if (utcb->head.mtr.untyped() < 3) return EPROTO;

      // sanitize the request
      char *request = reinterpret_cast<char *>(utcb->msg + 2);
      unsigned request_len = sizeof(unsigned)*(utcb->head.mtr.untyped() - 2);
      request[request_len-1] = 0;
      request_len = strnlen(request, request_len);

      // search for an allowed namespace
      char * cmdline = strstr(_modinfo[clientnr].cmdline, "namespace::");
      if (!cmdline) return EPERM;
      cmdline += 11;
      unsigned namespace_len = strcspn(cmdline, " \t");

      QuotaGuard<ServerData> guard1(utcb, clientnr, "mem", request_len + namespace_len + 1);
      QuotaGuard<ServerData> guard2(utcb, clientnr, "cap", 1, &guard1);
      check1(res, res = guard2.status());
      check1(res, (res = _server.alloc_client_data(utcb, sdata, clientnr)));
      guard2.commit();

      sdata->len = namespace_len + request_len + 1;
      sdata->name = new char[sdata->len];
      memcpy(sdata->name, cmdline, namespace_len);
      memcpy(sdata->name + namespace_len, request, request_len);
      sdata->name[sdata->len - 1] = 0;
      sdata->cpu  = utcb->msg[1];
      sdata->pt   = utcb->get_received_cap();

      for (ServerData * s2 = _server.next(); s2; s2 = _server.next(s2))
	if (s2->len == sdata->len && !memcmp(sdata->name, s2->name, sdata->len) && sdata->cpu == s2->cpu && sdata->pt != s2->pt) {
	  free_service(utcb, sdata);
	  return EEXISTS;
	}

      // wakeup clients that wait for us
      for (ClientData * c = _client.next(); c; c = _client.next(c))
	if (c->len == sdata->len-1 && !memcmp(c->name, sdata->name, c->len)) {
	  Logging::printf("notify client %x\n", c->parent_cap);
	  nova_semup(c->identity);
	}

      ParentProtocol::init_utcb(utcb, 1, 0, utcb->head.crd >> Utcb::MINSHIFT, sdata->identity);
      free_cap = false;
      return ENONE;
    }

  case ParentProtocol::TYPE_UNREGISTER:
    if ((res = _server.get_client_data(utcb, sdata))) return res;
    Logging::printf("unregister %s cpu %x\n", sdata->name, sdata->cpu);
    return free_service(utcb, sdata);

  case ParentProtocol::TYPE_GET_QUOTA:
    // XXX guid support
  default:
    return EPROTO;
  }
}


PT_FUNC(do_parent,
	// we lock to protect our datastructures and the malloc implementation
	SemaphoreGuard l(_lock);

	// we handle ourself as module 0!
	if (pid < CLIENT_PT_OFFSET) pid += CLIENT_PT_OFFSET;
	unsigned short client = (pid - CLIENT_PT_OFFSET) >> CLIENT_PT_SHIFT;
	bool free_cap = utcb->get_received_cap();

	utcb->head.mtr_out = Mtd(1, 0);
	utcb->msg[0] = handle_parent(utcb, client, free_cap);
	if (free_cap)
	  nova_revoke(Crd(utcb->get_received_cap(), 0, DESC_CAP_ALL), true);
	else if (utcb->get_received_cap())
	  utcb->head.crd = (alloc_cap() << Utcb::MINSHIFT) | DESC_TYPE_CAP;
	return utcb->head.mtr_out.value();
	)
