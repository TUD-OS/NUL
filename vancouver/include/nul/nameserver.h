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
    TYPE_TESTIDENTIFY,
  } type;
  unsigned cpu;
  char name [MAX_NAME];
};


#ifdef NS_CLIENT
#include "sys/utcb.h"
#include "nul/baseprogram.h"
struct NameserverClient {

  static unsigned name_resolve(Utcb *utcb, const char *service, unsigned cap) {
    unsigned len = strlen(service);
    if (len >= MessageNameserver::MAX_NAME-1) return EPROTO;

    TemporarySave<Utcb::HEADER_SIZE + sizeof(MessageNameserver) / sizeof(unsigned)> save(utcb);

    utcb->head.crd    = (cap << Utcb::MINSHIFT) | 3;
    while (1) {
      unsigned res;
      MessageNameserver *msg = reinterpret_cast<MessageNameserver *>(utcb->msg);
      msg->type = MessageNameserver::TYPE_RESOLVE;
      memcpy(msg->name, service, len);
      memset(msg->name + len, 0, MessageNameserver::MAX_NAME - len);

      check1(res, res = nova_call(MessageNameserver::PT_NS_BASE + Cpu::cpunr(), Mtd(sizeof(MessageNameserver) / sizeof(unsigned), 0)));
      if (utcb->msg[0] != ERETRY) return utcb->msg[0];

      // XXX this should be an get-all-events
      nova_semdown(MessageNameserver::PT_NS_SEM);
    }
    return utcb->msg[0];
  }


  static unsigned name_register(Utcb *utcb, const char *service, unsigned cap, unsigned cpu) {
    unsigned len = strlen(service);
    if (len >= MessageNameserver::MAX_NAME) return EPROTO;
    TemporarySave<Utcb::HEADER_SIZE + sizeof(MessageNameserver) / sizeof(unsigned)> save(utcb);

    MessageNameserver *msg = reinterpret_cast<MessageNameserver *>(utcb->msg);
    msg->type = MessageNameserver::TYPE_REGISTER;
    msg->cpu = cpu;
    memcpy(msg->name, service, len);
    memset(msg->name + len, 0, MessageNameserver::MAX_NAME - len);

    utcb->head.mtr = Mtd(sizeof(MessageNameserver) / sizeof(unsigned), 0);
    BaseProgram::add_mappings(utcb, false, cap << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, 0, 0x1c | DESC_TYPE_CAP);
    unsigned res;
    check1(res, res = nova_call(MessageNameserver::PT_NS_BASE + Cpu::cpunr(), utcb->head.mtr));
    return utcb->msg[0];
  };

  static unsigned name_identify(Utcb *utcb, unsigned &id) {
    TemporarySave<Utcb::HEADER_SIZE + sizeof(MessageNameserver) / sizeof(unsigned)> save(utcb);

    MessageNameserver *msg = reinterpret_cast<MessageNameserver *>(utcb->msg);
    msg->type = MessageNameserver::TYPE_TESTIDENTIFY;
    utcb->head.mtr = Mtd(sizeof(MessageNameserver) / sizeof(unsigned), 0);
    BaseProgram::add_mappings(utcb, false, MessageNameserver::PT_NS_SEM << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, 0, DESC_TYPE_CAP, false);
    unsigned res;
    check1(res, res = nova_call(MessageNameserver::PT_NS_BASE + Cpu::cpunr(), utcb->head.mtr));
    id = utcb->msg[1];
    return utcb->msg[0];
  }
};

#else

#include "service/lifo.h"
#include "service/logging.h"
#include "service/helper.h"
class Utcb;

template <class T>
class Nameserver  {
  struct Entry {
    Entry    * next;
    void     * client;
    unsigned   pt;
    unsigned   cpu;
    char     * sname;
  } * _head;


  void debug_dump(const char *func) {
    Logging::printf("%s:\n", func);
    Entry **prev = &_head;
    for (unsigned i=0; *prev; i++, prev = &(*prev)->next) {
      Logging::printf("\t%2x: %s\n", i, (*prev)->sname);
    }
  }


  void free_entry(Entry *service) {
    unsigned slen = strlen(service->sname)+1;
    T::free_portal(service->pt);
    T::account_resource(service->client, -sizeof(Entry) - slen);
    delete service->sname;
    delete service;
  }
public:
  unsigned resolve_name(Utcb *utcb, char *client_cmdline) {
    // should not get a mapping
    check1(T::set_error(utcb, EPROTO), T::get_received_cap(utcb));

    // sanitize the request
    MessageNameserver * msg = reinterpret_cast<MessageNameserver *>(T::get_message(utcb));
    msg->name[sizeof(msg->name) - 1] = 0;
    unsigned msg_len = strlen(msg->name);
    unsigned cpu = Cpu::cpunr();

    /**
     * Parse the cmdline for "name::" prefixes and check whether the
     * postfix matches the requested name.
     */
    char * cmdline = strstr(client_cmdline, "name::");
    bool found = false;
    while (cmdline) {
      cmdline += 6;
      unsigned namelen = strcspn(cmdline, " \t");

      Logging::printf("[%u] request for %x %x| '%s' | '%.*s'\n", Cpu::cpunr(), msg_len, namelen, msg->name, namelen, cmdline);
      if ((msg_len > namelen) || (0 != memcmp(cmdline + namelen - msg_len, msg->name, msg_len))) {
	cmdline = strstr(cmdline + namelen, "name::");
	continue;
      }

      found = true;

      /**
       * We found a postfix match in cmdline - "name::nspace:service".
       * Check now whether "nspace:service" is registered.
       */
      for (Entry * service = _head; service; service = service->next)
	if (service->cpu == cpu && !memcmp(service->sname, cmdline, namelen)) {
	  Logging::printf("[%u] request %s service %x,%x %s from %p\n", Cpu::cpunr(), msg->name, service->cpu, service->pt, service->sname, service->client);
	  return T::reply_with_cap(utcb, service->pt);
	}
	else
	  Logging::printf("[%u] request %s vs %s\n", Cpu::cpunr(), msg->name, service->sname);

      // Get the next alternative name...
      cmdline = strstr(cmdline + namelen, "name::");
    }
    Logging::printf("resolve '%s' - %s\n", msg->name, found ? "retry" : "not allowed");
    // should we retry because there could be a name someday, or don't we have the permission
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
    service->next  = 0;
    service->pt  = T::get_received_cap(utcb);
    service->cpu = msg->cpu;
    service->client = client;
    service->sname = new char[msg_len + namespace_len + 1];
    memcpy(service->sname, cmdline, namespace_len);
    memcpy(service->sname + namespace_len, msg->name, msg_len);
    service->sname[namespace_len + msg_len] = 0;
    Logging::printf("[%u] registered %x,%x %s for %p\n", Cpu::cpunr(), service->cpu, service->pt, service->sname, service->client);

    // check that nobody has registered the name already
    Entry **prev = &_head;
    while (*prev) {
      if (!strcmp(service->sname, (*prev)->sname) && service->cpu == (*prev)->cpu) {
	free_entry(service);
	return T::set_error(utcb, EEXISTS);
      }
      prev = &(*prev)->next;
    }

    // enqueue at the end of the list
    *prev = service;

    debug_dump(__func__);
    return T::set_error(utcb, ENONE);
  }


  /**
   * Delete all services from a client.
   */
  void delete_client(void *client) {

    Entry **prev = &_head;
    while (*prev)  {
      Entry *service = *prev;
      if (service->client == client) {
	*prev = service->next;
	Logging::printf("[%u] delete service %x,%x %s from %p\n", Cpu::cpunr(), service->cpu, service->pt, service->sname, service->client);
	free_entry(service);
      }
      else
	prev = &service->next;
    }
    debug_dump(__func__);
  }
};
#endif
