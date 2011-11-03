// -*- Mode: C++ -*-

#pragma once

#include <nul/parent.h>
#include <nul/generic_service.h>
#include <nul/capalloc.h>
#include <nul/program.h>
#include <sigma0/console.h>

#include "queue.h"

class BaseService {

protected:
  struct BaseSession : public Queue<BaseSession>::ListElement {
    cap_sel          sm_pseudonym;
    const cap_sel    pt;
    Closure          closure;

    // Called during session destruction when no worker holds a
    // reference to this session object anymore.
    virtual void cleanup() { }

    explicit BaseSession(cap_sel pt) : pt(pt) { }
  };

  Queue<BaseSession>     _free_sessions;

  struct per_cpu {
    cap_sel ec_service;         // session life-cycle (open, close)
    cap_sel pt_service;         // runs on ec_service

    Utcb   *utcb_client;
    cap_sel ec_client;          // normal client<->server interaction
    cap_sel pt_flush;           // drain clients

    // Structure of this list is only modified in ec_service!
    BaseSession *sessions;

    BaseSession *find_session(cap_sel sm_pseudonym)
    {
      for (BaseSession *c = sessions; c != NULL; c = c->next)
        if (c->sm_pseudonym == sm_pseudonym)
          return c;
      return NULL;
    }

    void remove_session(BaseSession *s)
    {
      if (sessions == s) {
        sessions = sessions->next;
      } else for (BaseSession *c = sessions; c != NULL; c = c->next) {
          if (c->next == s) {
            c->next = s->next;
            return;
          }
        }
    }

  } _per_cpu[Config::MAX_CPUS];

  virtual cap_sel alloc_cap() = 0;
  virtual void  dealloc_cap(cap_sel c) = 0;
  virtual cap_sel create_ec(phy_cpu_no cpu, Utcb **utcb_out) = 0;

  virtual BaseSession *new_session() = 0;

  static unsigned flush_func()
  {
    return 0;
  }

  void garbace_collect(per_cpu &local)
  {
    // This method can only run in ec_service. It modifies the session
    // list while traversing it, which is no problem as there is no
    // concurrency. Remember: Session list modifications only happen
    // in ec_service.

    for (BaseSession *s = local.sessions; s != NULL; s = s->next) {
      if (nova_lookup(Crd(s->sm_pseudonym, 0, DESC_CAP_ALL)).value() == 0) {
        Logging::printf("Garbage collect session %p\n", s);
        close_session(local, s);
      }
    }
  }

  virtual void close_session(per_cpu &local, BaseSession *s)
  {
    // This has to run in ec_service!

    // Stop people from entering ec_client with this particular
    // session object.
    nova_revoke(Crd(s->pt, 0, DESC_CAP_ALL), true);
    // Don't free the session cap selector. It is reused.

    // We don't need the pseudonym anymore either.
    nova_revoke(Crd(s->sm_pseudonym, 0, DESC_CAP_ALL), true);
    dealloc_cap(s->sm_pseudonym);
    

    // Ensure that no one holds a pointer to the session object by
    // helping everyone getting through it.
    unsigned res = nova_call(local.pt_flush);
    assert(res == NOVA_ESUCCESS);

    // Now no one can use the session portal anymore and neither is
    // anyone still executing in ec_client who could have a reference
    // to the session object. Cleanup!
    local.remove_session(s);
    _free_sessions.enqueue(s);
    Logging::printf("Closed %p\n", s);
  }

public:
  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
  {
    unsigned op;
    check1(EPROTO, input.get_word(op));

    per_cpu &local = _per_cpu[BaseProgram::mycpu()];
    unsigned res;

    switch (op) {
    case ParentProtocol::TYPE_OPEN: {
      BaseSession *s = local.find_session(input.received_cap());

      if (s) {
        //Logging::printf("REOPEN: %p\n", s);
        goto map_session;
      }

      // Garbage collect everytime (we might want to change this)
      garbace_collect(local);

      s = new_session();
      if (s == NULL) return ERESOURCE;
      // pt and closure are already set in s

      res = nova_create_pt(s->pt, local.ec_client,
                           s->closure.value(), 0);
      assert(res == NOVA_ESUCCESS);

      // No synchronization needed. Only modified in this EC
      s->sm_pseudonym = input.received_cap();
      s->next = local.sessions;
      MEMORY_BARRIER;
      local.sessions = s;
      
      map_session:
      utcb << Utcb::TypedMapCap(s->pt);
      return ENONE;
    }
    case ParentProtocol::TYPE_CLOSE: {
      BaseSession *s = local.find_session(input.received_cap());
      if (s == 0) return EPROTO;
      assert(s->sm_pseudonym == input.received_cap());
      close_session(local, s);
      return ENONE;
    }
    };

    return EPROTO;
  }

  unsigned alloc_crd()
  {
    return Crd(alloc_cap(), 0, DESC_CAP_ALL).value();
  }


  bool register_service(const char *service_name)
  {
    Logging::printf("Constructing service...\n");

    cap_sel  service_cap = alloc_cap();
    unsigned res;
    mword portal_func = reinterpret_cast<mword>(StaticPortalFunc<BaseService>::portal_func);
    mword flush_func = reinterpret_cast<mword>(BaseService::flush_func);
    for (phy_cpu_no i = 0; i < Global::hip.cpu_desc_count(); i++) {
      Hip_cpu const &cpu = Global::hip.cpus()[i];
      if (not cpu.enabled()) continue;

      _per_cpu[i].sessions = NULL;

      // Create client EC
      Utcb *utcb_client;
      _per_cpu[i].ec_client = create_ec(i, &utcb_client);
      utcb_client->head.crd           = 0;
      utcb_client->head.crd_translate = 0;
      _per_cpu[i].utcb_client = utcb_client;
      assert(_per_cpu[i].ec_client != 0);

      _per_cpu[i].pt_flush = alloc_cap();
      res = nova_create_pt(_per_cpu[i].pt_flush, _per_cpu[i].ec_client,
                           flush_func, 0);
      assert(res == NOVA_ESUCCESS);

      // Create service EC
      Utcb *utcb_service;
      _per_cpu[i].ec_service = create_ec(i, &utcb_service);
      assert(_per_cpu[i].ec_service != 0);
      utcb_service->head.crd = alloc_crd();
      utcb_service->head.crd_translate = Crd(0, 31, DESC_CAP_ALL).value();
      
      _per_cpu[i].pt_service = alloc_cap();
      res = nova_create_pt(_per_cpu[i].pt_service, _per_cpu[i].ec_service,
                           portal_func, 0);
      assert(res == NOVA_ESUCCESS);

      // Register service
      res = ParentProtocol::register_service(*BaseProgram::myutcb(), service_name, i,
                                             _per_cpu[i].pt_service, service_cap);
      if (res != ENONE)
        Logging::panic("Registering service on CPU%u failed.\n", i);
    }
           

    Logging::printf("Service registered.\n");
    return true;
  }

};


class ServiceProgram : public BaseService, public NovaProgram, public ProgramConsole
{
protected:

  virtual cap_sel alloc_cap()
  { return NovaProgram::alloc_cap(); }

  virtual void    dealloc_cap(cap_sel c)
  { return NovaProgram::dealloc_cap(c); }

  cap_sel create_ec(phy_cpu_no cpu, Utcb **utcb_out)
  {
    return create_ec4pt(this, cpu, 0, utcb_out);
  }

  ServiceProgram(const char *console_name = "service")
  {
    Hip *hip = &Global::hip;
    init(hip);
    init_mem(hip);

    console_init(console_name, new Semaphore(alloc_cap(), true));
  }

};

// EOF
