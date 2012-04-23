/*
 * Parent protocol implementation in sigma0.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2010-2011, Alexander Boettcher <boettcher@tudos.org>
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

#include <nul/generic_service.h>
#include <nul/capalloc.h>
#include <ctype.h>

PARAM_HANDLER(namespace, "namespace - namespace to register service names in (server parameter)") {}
PARAM_HANDLER(name, "name - allow client to access the given service") {}
PARAM_HANDLER(quota, "quota - used by parent protocol") {}

class s0_ParentProtocol : public CapAllocator {

private:

  // data per physical CPU number
  struct {
    unsigned  cap_ec_parent;
  } _percpu[MAXCPUS];

  /**
   * Missing: kill a client, mem+cap quota support
   */
  struct ClientData : public GenericClientData {
    char const    * name;
    unsigned        len;
    unsigned        singleton;

    /* TODO: Move this to s0_modules.h */
    struct Cmdline {
      const char *cmdline;
      const size_t len;
      Cmdline(const char *cmdline, size_t len) : cmdline(cmdline), len(len) {}

      /**
       * If value is not NULL, checks whether the command line
       * contains a (space separated) word in the form <prefix><value>
       * and if yes, a pointer to the value is returned.
       *
       * If the value is NULL, only the prefix is checked for precence
       * and the value is to be processed by the caller.
       */
      const char *has(const char *prefix, const char *value="") const {
        const char *pos;
        size_t plen = strlen(prefix);
        size_t vlen = value ? strlen(value) : 0;
        for (pos = strstr(cmdline, prefix);
             pos && pos+plen+vlen <= cmdline+len && (pos == cmdline || isspace(pos[-1]));
             pos = strstr(pos+1, prefix)) {
          pos += plen;
          if (value) {
            if (strncmp(pos, value, vlen) == 0 && (pos+vlen == cmdline+len || isspace(pos[vlen])))
              return pos;
          } else {
            return pos;
          }
        }
        return NULL;
      }
      const char *get(const char *prefix) const {
        return has(prefix, NULL);
      }
    };

    static unsigned get_quota(Utcb &utcb, unsigned parent_cap, const char *quota_name, long value_in, long *value_out=0) {
      Utcb::Frame input = utcb.get_nested_frame();
      //Logging::printf("get quota for '%s' amount %lx from %x for %x by %x\n", quota_name, value_in, parent_cap, input.identity(1), input.identity(0));
      if (!strcmp(quota_name, "mem") || !strcmp(quota_name, "cap")) return ENONE;

      if (!strcmp(quota_name, "guid")) {
        unsigned long s0_cmdlen;
        char const *pos, *cmdline = get_client_cmdline(input.identity(0), s0_cmdlen);
        if (cmdline && (pos = strstr(cmdline, "quota::guid")) && pos < cmdline + s0_cmdlen) {
          *value_out = get_client_number(parent_cap);
//        Logging::printf("send clientid %lx from %x\n", *value_out, parent_cap);
          return ENONE;
        }
        return ERESOURCE;
      }

      unsigned long cmdlen;
      char const *cmdline = get_client_cmdline(parent_cap, cmdlen);
      if (!cmdline) return ERESOURCE;
      Cmdline client_cmdline(cmdline, cmdlen);

      if (!strncmp(quota_name, "disk::", 6)) {
        const char *disk_name = quota_name + 6;
        unsigned len = strlen(disk_name);
        /* TODO: Pattern matching */
        if (client_cmdline.has("disk::", disk_name))
          return ENONE;

        if (len == 1 && isdigit(disk_name[0]) && client_cmdline.has("sigma0::drive:", disk_name)) /* Backward compatibility */
          return ENONE;

        return ERESOURCE;
      }

      if (strcmp(quota_name, "diskadd") == 0)
        return client_cmdline.has("diskadd") ? ENONE : ERESOURCE;

      return ERESOURCE;
    }
    void session_close(Utcb &utcb) {
      if (singleton) {
        //dealloc_cap(singleton);
        unsigned char res = nova_revoke(Crd(singleton, 0, DESC_CAP_ALL), true);
        assert(!res);
        singleton = 0;
      }
      name = 0;
      len = 0;
    }
  };

  struct ServerData : public ClientData {
    unsigned        cpu;
    unsigned        pt;
    char *          mem_revoke;
  };

  static char const * get_client_cmdline(unsigned identity, unsigned long &s0_cmdlen);
  static char * get_client_memory(unsigned identity, unsigned client_mem_revoke);

  typedef ClientDataStorage<ClientData, s0_ParentProtocol, false>::Guard GuardC;
  typedef ClientDataStorage<ServerData, s0_ParentProtocol, false>::Guard GuardS;

  ALIGNED(8) ClientDataStorage<ClientData, s0_ParentProtocol, false> session; /// client "registry" of sessions to services
  ALIGNED(8) ClientDataStorage<ServerData, s0_ParentProtocol, false> _server; /// Service "registry"

  static unsigned get_client_number(unsigned cap) {
    cap -= CLIENT_PT_OFFSET;
    if ((cap % (1 << CLIENT_PT_SHIFT)) != ParentProtocol::CAP_PARENT_ID) return ~0;
    return cap >> CLIENT_PT_SHIFT;
  }

  unsigned get_portal(Utcb &utcb, unsigned cap_client, unsigned &portal) {
    ClientData *cdata;
    ServerData *sdata;
    unsigned res;

    GuardC guard_c(&session);
    if ((res = session.get_client_data(utcb, cdata, cap_client))) return res;
    //Logging::printf("\tfound session cap %x for client %x %.*s\n", cap_client, cdata->pseudonym, cdata->len, cdata->name);

    GuardS guard_s(&_server);
    for (sdata = _server.next(); sdata; sdata = _server.next(sdata))
       if (sdata->cpu == utcb.head.nul_cpunr && cdata->len == sdata->len-1 && !memcmp(cdata->name, sdata->name, cdata->len)) {
         // check that the server portal still exists, if not free the server-data and tell the client to retry
         unsigned crdout;
         if (nova_syscall(NOVA_LOOKUP, Crd(sdata->pt, 0, DESC_CAP_ALL).value(), 0, 0, 0, &crdout) || !crdout) {
           free_service(utcb, sdata);
           return ERETRY;
         }
         portal = sdata->pt;
         return ENONE;
       }
    //Logging::printf("s0: we have no service portal for '%10s...' yet - retry later\n", cdata->name);
    // we do not have a server portal yet, thus tell the client to retry later
    return ERETRY;
  }

  /**
   * Check whether a client has the permission to access a service.
   *
   * Parse the client's cmdline for "name::" prefixes and check whether the
   * postfix matches the requested name. In the current
   * implementation, it is only checked whether the trailing part of
   * the postfix (without the namespace) matches the requested name.
   *
   * @param identity The identity of the client (i.e. ParentProtocol::CAP_PARENT_ID)
   * @param request Name of the requested service
   * @param request_len Length of the name
   * @param instance Which instance of the service is requested
   * @param[out] cmdline Pointer to the postfix of the matching "name::" parameter (including the namespace)
   * @param[out] namelen The length the postfix
   */
  unsigned check_permission(
    unsigned identity, const char *request, unsigned request_len,
    unsigned instance, char const * &cmdline, unsigned &namelen)
  {
    unsigned long s0_cmdlen;
    cmdline = get_client_cmdline(identity, s0_cmdlen);
    char const * cmdline_end = cmdline + s0_cmdlen;
    if (!cmdline) return EPROTO;
    cmdline = strstr(cmdline, "name::");
    while (cmdline && cmdline < cmdline_end) {
      cmdline += 6;
      namelen = strcspn(cmdline, " \t\r\n\f");
      if ((request_len > namelen) || (0 != memcmp(cmdline + namelen - request_len, request, request_len)) ||
          (*(cmdline + namelen - request_len - 1) != '/')) {
        cmdline = strstr(cmdline + namelen, "name::");
        continue;
      }
      if (instance--) {
        cmdline = strstr(cmdline + namelen, "name::");
        continue;
      }
      return ENONE;
    }
    //Logging::printf("s0: client has no permission to access service '%s'\n", request);
    // we do not have the permissions
    return EPERM;
  }

  unsigned free_service(Utcb &utcb, ServerData *sdata) {
    if (sdata->pt) dealloc_cap(sdata->pt);
    if (sdata->name) delete [] sdata->name;
    ServerData::get_quota(utcb, sdata->pseudonym, "cap", -1);
    ServerData::get_quota(utcb, sdata->pseudonym, "mem", -sdata->len);
    return _server.free_client_data(utcb, sdata, this);
  }

  void notify_service(Utcb &utcb, ClientData *c) {

    //revoke identity cap so that services can detect that client is gone
    unsigned res = nova_revoke(Crd(c->get_identity(), 0, DESC_CAP_ALL), false);
    assert(res == ENONE);

    {
      GuardS guard_s(&_server);
      for (ServerData * sdata = _server.next(); sdata; sdata = _server.next(sdata))
        if (sdata->cpu == utcb.head.nul_cpunr && c->len == sdata->len-1 &&
            !memcmp(c->name, sdata->name, c->len) && sdata->mem_revoke)
              *sdata->mem_revoke = 1; //flag revoke
    }
  }

public:
  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid) {
    unsigned res;
    unsigned op;
    if (input.get_word(op)) return EPROTO;
    //Logging::printf("parent request words %x type %x id %x+%x\n", input.untyped(), op, input.identity(), input.identity(1));
    switch (op) {
    case ParentProtocol::TYPE_OPEN:
      {
        unsigned instance, request_len, service_name_len;
        const char *request, *service_name;
        ClientData *cdata;

        if (input.get_word(instance) || !(request = input.get_zero_string(request_len))) return EPROTO;
        if (res = check_permission(input.identity(0), request, request_len, instance,
                                   service_name, service_name_len)) return res;

        // check whether such a session is already known from our client
        {
          GuardC guard_c(&session);
          for (ClientData * c = session.next(); c; c = session.next(c))
            if (c->name == service_name && c->pseudonym == input.identity()) {
              utcb << Utcb::TypedMapCap(c->get_identity());
              //Logging::printf("pp: has already a cap %s identity=0x%x pseudo=0x%x %10s\n", request, c->get_identity(), c->pseudonym, service_name);
              return ENONE;
            }
        }

        res = session.alloc_client_data(utcb, cdata, input.identity()/*becomes cdata->pseudonym*/, this);
        if (res) {
          if (res != ERESOURCE) return res;

          unsigned count = 0;
          GuardC guard_c(&session, utcb, this);
          ClientData * data = session.get_invalid_client(utcb, this);
          while (data) {
            //Logging::printf("pp: found dead client - freeing datastructure\n");
            count++;

            notify_service(utcb, data);

            session.free_client_data(utcb, data, this);
            data = session.get_invalid_client(utcb, this, data);
          }
          if (count > 0) return ERETRY;
          else return ERESOURCE;
        }

        cdata->name = service_name;
        MEMORY_BARRIER; //make sure cdata is complete, len could be used in parallel to decide to compare against cdata->name already
        cdata->len  = service_name_len;
        //Logging::printf("pp: created new client  service='%.*s' identity=%#x pseudo=%#x\n", cdata->len, cdata->name, cdata->get_identity(), cdata->pseudonym);
        utcb << Utcb::TypedMapCap(cdata->get_identity());
        return ENONE;
      }

    case ParentProtocol::TYPE_CLOSE:
      {
        GuardC guard_c(&session, utcb, this);
        ClientData *cdata;

        if ((res = session.get_client_data(utcb, cdata, input.identity()))) return res;
        //Logging::printf("pp: close session for service='%.*s' identity=%#x pseudo=%#x\n", cdata->len, cdata->name, cdata->get_identity(), cdata->pseudonym);
        notify_service(utcb, cdata); // XXX: Race - see below
        return session.free_client_data(utcb, cdata, this);
      }
    case ParentProtocol::TYPE_GET_PORTAL:
      {
        unsigned portal, res;
        //Logging::printf("pp: get portal for id %#x\n", input.identity());
        if (res = get_portal(utcb, input.identity(), portal)) return res;
        utcb << Utcb::TypedMapCap(portal);
        return res;
      }
    case ParentProtocol::TYPE_REGISTER:
      {
        ServerData *sdata;
        char *request;
        unsigned cpu, request_len;
        if (input.get_word(cpu) || !(request = input.get_zero_string(request_len))) return EPROTO;

        // search for an allowed namespace
        unsigned long s0_cmdlen;
        char const * cmdline, * _cmdline = get_client_cmdline(input.identity(0), s0_cmdlen);
        if (!_cmdline) return EPROTO;
        //Logging::printf("\tregister client %x @ cpu %x _cmdline '%.10s' servicename '%.10s'\n", input.identity(), cpu, _cmdline, request);
        cmdline = strstr(_cmdline, "namespace::");
        if (!cmdline || cmdline > _cmdline + s0_cmdlen) return EPERM;
        cmdline += sizeof("namespace::")-1;
        unsigned namespace_len = strcspn(cmdline, " \t");

        QuotaGuard<ServerData> guard1(utcb, input.identity(), "mem", request_len + namespace_len + 1);
        QuotaGuard<ServerData> guard2(utcb, input.identity(), "cap", 1, &guard1);
        check1(res, res = guard2.status());
        res = _server.alloc_client_data(utcb, sdata, input.identity()/*becomes sdata->pseudonym*/, this);
        if (res == ENONE) guard2.commit();
        else {
          if (res != ERESOURCE) return res;

          unsigned count = 0;
          GuardS guard_s(&_server, utcb, this);
          ServerData * sdata = _server.get_invalid_client(utcb, this);
          while (sdata) {
            Logging::printf("s0: found dead server - freeing datastructure\n");
            count++;
            free_service(utcb, sdata);
            sdata = _server.get_invalid_client(utcb, this, sdata);
          }
          if (count > 0) return ERETRY;
          else return ERESOURCE;
        }

        unsigned slen = namespace_len + request_len + 1;
        char * tmp = new char[slen];
        if (!tmp) {
          free_service(utcb, sdata);
          return ERESOURCE;
        }

        memcpy(tmp, cmdline, namespace_len);
        memcpy(tmp + namespace_len, request, request_len);
        tmp[slen - 1] = 0;
        sdata->cpu  = cpu;
        sdata->pt   = input.received_cap();

        unsigned client_mem_revoke;
        if (!input.get_word(client_mem_revoke))
          sdata->mem_revoke = get_client_memory(input.identity(), client_mem_revoke);

        sdata->name = tmp;
        MEMORY_BARRIER; //make sure sdata is complete, len could be used in parallel to decide to compare against sdata->name already
        sdata->len  = slen;
        {
          GuardS guard_s(&_server);
          for (ServerData * s2 = _server.next(); s2; s2 = _server.next(s2))
            if (s2->len == sdata->len && !memcmp(sdata->name, s2->name, sdata->len) && sdata->cpu == s2->cpu && sdata->pt != s2->pt) {
              free_service(utcb, sdata);
              return EEXISTS;
            }
        }

        // wakeup clients that wait for us
        {
          GuardC guard_c(&session);
          for (ClientData * c = session.next(); c; c = session.next(c))
          if (c->len == sdata->len-1 && !memcmp(c->name, sdata->name, c->len)) {
            //Logging::printf("\tnotify client %x\n", c->pseudonym);
            unsigned res = nova_semup(c->get_identity());
            assert(res == ENONE); //c->identity is allocated by us
          }
        }

        utcb << Utcb::TypedMapCap(sdata->get_identity());
        free_cap = false;
        return ENONE;
      }

    case ParentProtocol::TYPE_UNREGISTER:
      {
        GuardS guard_s(&_server, utcb, this);
        ServerData *sdata;

        if ((res = _server.get_client_data(utcb, sdata, input.identity()))) return res;
        //Logging::printf("pp: unregister %s cpu %x\n", sdata->name, sdata->cpu);
        return free_service(utcb, sdata);
      }
    case ParentProtocol::TYPE_SINGLETON:
      {
        unsigned op;
        if (input.get_word(op)) return EPROTO;

        ClientData *cdata;
        GuardC guard_c(&session);
        if ((res = session.get_client_data(utcb, cdata, input.identity(1)))) return res;

        if (op == 1U) //set
        {
          unsigned cap = input.received_cap();
          if (!cap) return EPROTO;

          cdata->singleton = cap;
          free_cap = false;
        } else if (op == 2U) //get
          if (cdata->singleton)
            utcb << Utcb::TypedIdentifyCap(cdata->singleton);
        else return EPROTO;

        return ENONE;
      }
    case ParentProtocol::TYPE_GET_QUOTA:
      {
        ClientData *cdata;
        GuardC guard_c(&session);
        if ((res = session.get_client_data(utcb, cdata, input.identity(1))))  return res;

        long invalue;
        char *request;
        unsigned request_len;
        if (input.get_word(invalue) || !(request = input.get_zero_string(request_len))) return EPROTO;

        long outvalue = invalue;
        res = ClientData::get_quota(utcb, cdata->pseudonym, request,  invalue, &outvalue);
        utcb << outvalue;
        return res;
      }
    case ParentProtocol::TYPE_REQ_KILL:
      {
        GuardC guard_c(&session, utcb, this);

        // find all sessions for a given client
        for (ClientData * c = session.next(); c; c = session.next(c)) {
          if (c->pseudonym != input.identity(1)) continue;

          notify_service(utcb, c); // XXX: Race - clients can notice which client was killed only after we revoke pseudonym, i.e. after return from this block (Guard)

          //Logging::printf("s0: freeing session to service on behalf of a dying client\n");
          res = session.free_client_data(utcb, c, this);
          assert(res == ENONE);
        }
        return ENONE;
      }
    case ParentProtocol::TYPE_SIGNAL:
      {
        unsigned value;
        if (input.get_word(value)) return EPROTO;
        /* The child cannot call semup, because it has insufficient permissions. */
        return nova_semup(input.identity());
      }
    default:
      return EPROTO;
    }
  }

  unsigned _cap_all_start, _cap_all_order;

  /** The first set of caps is used by ParentProtocol for internal
   * stuff, the second range is the client capability range, which
   * includes the first. We need those two to be in a single naturally
   * aligned block in order to create a translation window that spans
   * both.
   */
  s0_ParentProtocol(unsigned cap_start, unsigned cap_order, unsigned cap_all_start, unsigned cap_all_order)
    : CapAllocator(cap_start, cap_start, cap_order), _cap_all_start(cap_all_start), _cap_all_order(cap_all_order)
  {
    assert((cap_all_start & ((1U << cap_all_order)-1)) == 0);
  }

  unsigned alloc_crd() { return Crd(alloc_cap(), 0, DESC_CAP_ALL).value(); }

  template <class T>
  unsigned create_pt_per_client(unsigned base, T * __sigma0) {
    for (unsigned lcpu = 0; lcpu < __sigma0->_numcpus; lcpu++) {
      assert(lcpu < MAXCPUS);
      unsigned pcpu = __sigma0->_cpunr[lcpu];
      assert(pcpu < MAXCPUS);
      check1(3, nova_create_pt(base + ParentProtocol::CAP_PT_PERCPU + pcpu,
                               _percpu[pcpu].cap_ec_parent,
                               reinterpret_cast<unsigned long>(StaticPortalFunc<s0_ParentProtocol>::portal_func),
                               0));
    }
    return 0;
  }

  template <class T>
  unsigned create_threads(T * __sigma0) {
    for (unsigned lcpu = 0; lcpu < __sigma0->_numcpus; lcpu++) {
      Utcb *utcb = 0;
      unsigned pcpu = __sigma0->_cpunr[lcpu];
      assert(pcpu < MAXCPUS);
      _percpu[pcpu].cap_ec_parent = __sigma0->create_ec4pt(this, pcpu, __sigma0->_percpu[lcpu].exc_base, &utcb);
      utcb->head.crd = alloc_crd();
      utcb->head.crd_translate = Crd(_cap_all_start, _cap_all_order, DESC_CAP_ALL).value();

      // create parent portals
      check1(2, nova_create_pt(ParentProtocol::CAP_PT_PERCPU + pcpu, _percpu[pcpu].cap_ec_parent,
                               reinterpret_cast<unsigned long>(StaticPortalFunc<s0_ParentProtocol>::portal_func), 0));
    }
    return 0;
  }
};
