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
 * Missing: kill a client, get_quota, lookup-portal before mapping it back to check wether the server is still active or not
 */

static void dealloc_cap(unsigned cap) {
  nova_revoke(Crd(cap, 0, DESC_CAP_ALL), true);
  // XXX add it back to the cap-allocator
};

bool account_resource(unsigned clientnr, long amount) {
  // XXX unimplemented
  return true;
}


struct NameService {
  NameService   * next;
  char          * sname;
  unsigned        nlen;
  unsigned       clientnr;
  unsigned       parent_cap;
  unsigned       cpu;
  unsigned       pt;
} * _ns_service;


struct NameClient {
  NameClient  * next;
  char        * sname;
  unsigned      slen;
  unsigned      clientnr;
  unsigned      parent_cap;
} * _ns_clients;


/**
 * Resolve a name.
 */
unsigned session_open(Utcb *utcb, Mtd mtr, unsigned clientnr, char *client_cmdline) {
  if (mtr.untyped() <= 1) return EPROTO;
  char *request = reinterpret_cast<char *>(utcb->msg+1);
  unsigned request_len = sizeof(unsigned)*(mtr.untyped()-1);
  request[request_len-1] = 0;
  request_len = strnlen(request, request_len);

  /**
   * Parse the cmdline for "name::" prefixes and check whether the
   * postfix matches the requested name.
   */
  char * cmdline = strstr(client_cmdline, "name::");
  while (cmdline) {
    cmdline += 6;
    unsigned namelen = strcspn(cmdline, " \t\r\n\f");
    if ((request_len > namelen) || (0 != memcmp(cmdline + namelen - request_len, request, request_len))) {
      cmdline = strstr(cmdline + namelen, "name::");
      continue;
    }

    // check whether such a session is already known from our client
    for (NameClient * client = _ns_clients; client; client = client->next)
      if (client->sname == cmdline && client->clientnr == clientnr)
	return EEXISTS;

    // we do not have such a session yet, thus allocate one
    if (!account_resource(clientnr, sizeof(NameClient))) return ERESOURCE;
    NameClient *c = new NameClient();
    c->sname = cmdline;
    c->slen  = namelen;
    c->clientnr = clientnr;
    c->parent_cap = alloc_cap();
    nova_create_sm(c->parent_cap);
    c->next  = _ns_clients;
    _ns_clients = c;
    ParentProtocol::init_utcb(utcb, 1, c->parent_cap, utcb->head.crd >> Utcb::MINSHIFT, true);
    Logging::printf("return cap %x to client %x\n", c->parent_cap, clientnr);
    return ENONE;
  }

  // we do not have the permissions
  return EPERM;
}


unsigned session_close(Utcb *utcb, Mtd mtr) {
  unsigned identity = utcb->get_identity(mtr);
  if (!identity) return EEXISTS;
  Logging::printf("close cap %x\n", identity);
  for (NameClient **prev = &_ns_clients; *prev; prev = &(*prev)->next) {
    NameClient *client = *prev;
    if (client->parent_cap == identity) {
      Logging::printf("close cap %x for name '%.*s'\n", identity, client->slen, client->sname);
      *prev = client->next;
      dealloc_cap(client->parent_cap);
      account_resource(client->clientnr, -sizeof(NameClient));
      delete client;
      return ENONE;
    }
  }
  // wrong identity
  return EPROTO;
};


unsigned request_portal(Utcb *utcb, Mtd mtr) {
  unsigned identity = utcb->get_identity(mtr);
  if (!identity) return EEXISTS;
  unsigned cpu = Cpu::cpunr();
  for (NameClient * client = _ns_clients; client; client = client->next)
    if (client->parent_cap == identity) {
      Logging::printf("found session cap %x for client %x %.*s\n", identity, client->clientnr, client->slen, client->sname);
      for (NameService * service = _ns_service; service; service = service->next) {
	Logging::printf("service %s cpu %x nlen %x slen %x\n", service->sname, service->cpu, service->nlen, client->slen);;
	if (cpu == service->cpu && client->slen == service->nlen-1 && !memcmp(client->sname, service->sname, client->slen)) {
	  Logging::printf("found service cap %x\n", service->pt);
	  // XXX check that the service->pt still exists
	  ParentProtocol::init_utcb(utcb, 1, service->pt, utcb->head.crd >> Utcb::MINSHIFT, true);
	  return ENONE;
	}
      }
      return ERETRY;
    }
  // wrong identity
  Logging::printf("wrong identity %x\n", identity);
  return EPROTO;
}



void free_service(NameService *service) {
  assert(!service->next);
  dealloc_cap(service->pt);
  dealloc_cap(service->parent_cap);
  account_resource(service->clientnr, -sizeof(NameService) - (service->nlen + 1));
  delete service->sname;
  delete service;
}


unsigned register_service(Utcb *utcb, Mtd mtr, unsigned clientnr, char *client_cmdline, bool &free_cap) {
  if (mtr.untyped() <= 2) return EPROTO;

  // sanitize the request
  char *request = reinterpret_cast<char *>(utcb->msg + 2);
  unsigned request_len = sizeof(unsigned)*(mtr.untyped() - 2);
  request[request_len-1] = 0;
  request_len = strnlen(request, request_len);

  // search for an allowed namespace
  char * cmdline = strstr(client_cmdline, "namespace::");
  if (!cmdline) return EPERM;
  cmdline += 11;
  unsigned namespace_len = strcspn(cmdline, " \t");

  // we have a namespace and need to check the limits.
  check1(ERESOURCE, !account_resource(clientnr, sizeof(NameService) + request_len + namespace_len + 1));

  // alloc and fill the structure.
  NameService *service = new NameService;
  service->nlen = namespace_len + request_len + 1;
  service->sname = new char[service->nlen];
  memcpy(service->sname, cmdline, namespace_len);
  memcpy(service->sname + namespace_len, request, request_len);
  service->sname[service->nlen - 1] = 0;
  service->clientnr   = clientnr;
  service->parent_cap = alloc_cap();
  nova_create_sm(service->parent_cap);
  service->cpu  = utcb->msg[1];
  service->pt   = utcb->get_received_cap(mtr);
  service->next = 0;


  // check that nobody has registered the name already
  NameService **prev = &_ns_service;
  for (; *prev; prev = &(*prev)->next)
    if (!memcmp(service->sname, (*prev)->sname, service->nlen) && service->cpu == (*prev)->cpu) {
      free_service(service);
      return EEXISTS;
    }

  // enqueue at the end of the list
  *prev = service;

  // wakeup clients that wait for us
  for (NameClient * client = _ns_clients; client; client = client->next)
    if (client->slen == service->nlen-1 && !memcmp(client->sname, service->sname, client->slen)) {
      Logging::printf("notify client %x\n", client->clientnr);
      nova_semup(client->parent_cap);
    }

  ParentProtocol::init_utcb(utcb, 1, service->parent_cap, utcb->head.crd >> Utcb::MINSHIFT, true);
  free_cap = false;
  return ENONE;
}


unsigned unregister_service(Utcb *utcb, Mtd mtr) {
  unsigned identity = utcb->get_identity(mtr);
  if (!identity) return EEXISTS;
  for (NameService **prev = &_ns_service; *prev; prev = &(*prev)->next) {
    NameService *service = *prev;
    if (service->parent_cap == identity) {
      *prev = service->next;
      service->next = 0;
      free_service(service);
      return ENONE;
    }
  }
  // wrong identity
  return EPROTO;

}

unsigned handle_parent(Utcb *utcb, unsigned clientnr, Mtd mtr, bool &free_cap) {
  Logging::printf("parent request client %x mtr %x/%x type %x ", clientnr, mtr.untyped(), mtr.typed(), utcb->msg[0]);
  if (mtr.untyped() < 1) return EPROTO;
  switch (utcb->msg[0]) {
  case ParentProtocol::TYPE_OPEN:
    return session_open(utcb, mtr, clientnr, _modinfo[clientnr].cmdline);
  case ParentProtocol::TYPE_CLOSE:
    return session_close(utcb, mtr);
  case ParentProtocol::TYPE_REQUEST:
    return request_portal(utcb, mtr);
  case ParentProtocol::TYPE_REGISTER:
    return register_service(utcb, mtr, clientnr, _modinfo[clientnr].cmdline, free_cap);
  case ParentProtocol::TYPE_UNREGISTER:
    return unregister_service(utcb, mtr);
  default:
    return EPROTO;
  }
}


PT_FUNC(do_parent,
	// we lock for malloc and our datastructures
	SemaphoreGuard l(_lock);

	// we handle ourself as module 0!
	if (pid < CLIENT_PT_OFFSET) pid += CLIENT_PT_OFFSET;
	unsigned short client = (pid - CLIENT_PT_OFFSET)>> CLIENT_PT_SHIFT;
	Mtd mtr = utcb->head.mtr;
	bool free_cap = utcb->get_received_cap(mtr);

	utcb->head.mtr = Mtd(1, 0);
	utcb->msg[0] = handle_parent(utcb, client, mtr, free_cap);
	if (free_cap)
	  nova_revoke(Crd(utcb->get_received_cap(mtr), 0, DESC_CAP_ALL), true);
	else if (utcb->get_received_cap(mtr))
	  utcb->head.crd = (alloc_cap() << Utcb::MINSHIFT) | DESC_TYPE_CAP;
	Logging::printf(" =  %x\n", utcb->msg[0]);
	)
