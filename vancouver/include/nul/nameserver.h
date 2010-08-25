/*
 * Nameserver and client code.
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
#pragma once
#include "nul/error.h"
#include "service/string.h"


struct MessageNameserver {
  enum {
    MAX_NAME     = 64,
    PT_NS_SEM    = 255,
    PT_NS_BASE,
  };
  enum Type {
    TYPE_RESOLVE,
    TYPE_REGISTER,
  } type;
  unsigned cpu;
  char name [MAX_NAME];
};


#ifdef NS_CLIENT

#include "nul/baseprogram.h"
struct NameserverClient {

  static unsigned name_resolve(Utcb *utcb, const char *service, unsigned cap, unsigned cpu) {
    unsigned len = strlen(service);
    if (len >= MessageNameserver::MAX_NAME) return EPROTO;

    TemporarySave<Utcb::HEADER_SIZE + sizeof(MessageNameserver) / sizeof(unsigned)> save(utcb);

    MessageNameserver *msg = reinterpret_cast<utcb->msg>;
    msg->type = MessageNameserver::TYPE_RESOLVE;
    msg->cpu = cpu;
    memcpy(msg->name, service, len);
    memset(msg->name + len, MessageNameserver::MAX_NAME - len);

    utcb->head.crd    = (cap << Utcb::MINSHIFT) | 3;
    while (1) {
      unsigned res;
      check1(res, res = nova_call(PT_NS_BASE + Cpu::cpunr(), Mtd(sizeof(MessageNameserver), 0)));
      check1(utcb->msg[0], utcb->msg[0] != ERETRY);

      // XXX this should be an get-all-events
      nova_semdown(PT_NS_SEM);
    }
    return utcb->msg[0];
  }


  static unsigned name_register(Utcb *utcb, const char *service, unsigned cap, unsigned cpu) {
    unsigned len = strlen(service);
    if (len >= MessageNameserver::MAX_NAME) return EPROTO;
    TemporarySave<Utcb::HEADER_SIZE + sizeof(MessageNameserver) / sizeof(unsigned)> save(utcb);

    MessageNameserver *msg = reinterpret_cast<utcb->msg+1>;
    msg->type = MessageNameserver::TYPE_REGISTER;
    msg->cpu = cpu;
    memcpy(msg->name, service, len);
    memset(msg->name + len, MessageNameserver::MAX_NAME - len);

    utcb->head.mtr = Mtd(sizeof(MessageNameserver), 0);
    BaseProgram::add_mappings(utcb, false, cap << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, 0, 0x18 | DESC_TYPE_CAP);
    unsigned res;
    check1(res, res = nova_call(PT_NS_BASE + Cpu::cpunr(), utcb->head.mtr));
    return utcb->msg[0];
  };
};

#else

#include "service/lifo.h"
#include "service/logging.h"
#include "service/helper.h"
class Utcb;

template <class T>
class Nameserver  {
  struct Entry {
    Entry    * lifo_next;
    void     * client;
    unsigned   pt;
    unsigned   cpu;
    char     * sname;
  };
  AtomicLifo<Entry> _entries;

public:
  unsigned resolve_name(Utcb *utcb, char *client_cmdline) {
    // should not get a mapping
    check1(T::set_error(utcb, EPROTO), T::get_received_cap(utcb));

    // sanitize the request
    MessageNameserver * msg = reinterpret_cast<MessageNameserver *>(T::get_message(utcb));
    msg->name[sizeof(msg->name) - 1] = 0;
    unsigned msg_len = strlen(msg->name);

    /**
     * Parse the cmdline for "name::" prefixes and check whether the
     * postfix matches the requested name.
     */
    char * cmdline = strstr(client_cmdline, "name::");
    bool found = false;
    while (cmdline) {
      cmdline += 6;
      unsigned namelen = strcspn(cmdline, " \t");

      Logging::printf("[%u] request for %x %x | %s | %s\n", Cpu::cpunr(), msg_len, namelen, msg->name, cmdline + namelen - msg_len);
      if ((msg_len > namelen) || (0 != memcmp(cmdline + namelen - msg_len, msg->name, msg_len))) {
	cmdline = strstr(cmdline + namelen, "name::");
	continue;
      }

      found = true;

      /**
       * We found a postfix match in cmdline - "name::nspace:service".
       * Check now whether "nspace:service" is registered.
       */
      for (Entry * service = _entries.head(); service; service = service->lifo_next)
	if (service->cpu == msg->cpu && !memcmp(service->sname, cmdline, namelen)) {
	  Logging::printf("[%u] request %s service %x,%x %s from %p\n", Cpu::cpunr(), msg->name, service->cpu, service->pt, service->sname, service->client);
	  return T::reply_with_cap(utcb, service->pt);
	}

      // Get the next alternative name...
      cmdline = strstr(cmdline + namelen, "name::");
    }

    // should we retry because there could be a name someday, or is it useless?
    return T::set_error(utcb, found ? ERETRY : EPERM);
  }


  unsigned register_name(Utcb *utcb, void *client, char *client_cmdline) {

    // we need the mapping
    check1(T::set_error(utcb, EPROTO), !T::get_received_cap(utcb));

    // sanitize the request
    MessageNameserver * msg = reinterpret_cast<MessageNameserver *>(T::get_message(utcb));
    msg->name[sizeof(msg->name) - 1] = 0;
    unsigned msg_len = strlen(msg->name);

    // search for an allowed namespace
    char * cmdline = strstr(client_cmdline, "namespace::");
    check1(T::set_error(utcb, EPERM), !cmdline);
    cmdline += 11;
    unsigned namespace_len = strcspn(cmdline, " \t");

    // We have a namespace and need to check the limits.
    check1(T::set_error(utcb, ERESOURCE), !T::account_resource(client, sizeof(Entry) + msg_len + namespace_len + 1));

    // Alloc and fill the structure.
    Entry *service = new Entry;
    service->pt  = T::get_received_cap(utcb);
    service->cpu = msg->cpu;
    service->client = client;
    service->sname = new char[msg_len + namespace_len + 1];
    memcpy(service->sname, cmdline, namespace_len);
    memcpy(service->sname + namespace_len, msg->name, msg_len);
    service->sname[namespace_len + msg_len] = 0;
    Logging::printf("[%u] registered %x,%x %s for %p\n", Cpu::cpunr(), service->cpu, service->pt, service->sname, service->client);

    /**
     * We could check here whether somebody else has registered this
     * name already. But this can not be done atomically with the
     * enqueue.  We therefore omit this check and allow to register
     * names twice.  However "register" races with "deletion", thus
     * resolving is not deteministic with multiple entries with the
     * very same name.  The threads that register have to avoid such
     * situations!
     */
    _entries.enqueue(service);
    return T::set_error(utcb, ENONE);
  }


  /**
   * Delete all services from a client.
   */
  void delete_client(void *client) {

    Entry *next;
    for (Entry * service = _entries.dequeue_all(); service; service = next) {
      next = service->lifo_next;
      service->lifo_next = 0;
      if (service->client != client)
	_entries.enqueue(service);
      else {
	Logging::printf("[%u] delete service %x,%x %s from %p\n", Cpu::cpunr(), service->cpu, service->pt, service->sname, service->client);
	unsigned slen = strlen(service->sname)+1;
	T::free_portal(service->pt);
	T::account_resource(client, -sizeof(Entry) - slen);

	// XXX wait a grace period, as resolver on another CPU could iterate over the list
	delete service->sname;
	delete service;
      }
    }
  }
};
#endif
