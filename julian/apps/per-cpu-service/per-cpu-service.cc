#include <nul/program.h>
#include <sigma0/console.h>

#include <nul/parent.h>
#include <nul/generic_service.h>
#include <nul/capalloc.h>

#include "closure.h"
#include "client.h"


template<class T>
class Queue {

  T * volatile _head;

public:

  class ListElement {
  public:
    T * volatile next;
  };

  void enqueue(T *e) {
    T *head;
    do {
      head     = _head;
      e->next  = head;
    } while (not __sync_bool_compare_and_swap(&_head, head, e));

  }

  T *dequeue() {
    T *head;
    do {
      head = _head;
      if (head == NULL) return NULL;
    } while (not __sync_bool_compare_and_swap(&_head, head, head->next));
    return head;
  }

  Queue() : _head(NULL) { }
};

class PerCpuService : public NovaProgram, public ProgramConsole
{
  struct Session : Queue<Session>::ListElement {
    Session * volatile next;

    cap_sel          sm_pseudonym;
    const cap_sel    pt;
    Closure          closure;

    explicit Session(cap_sel pt) : pt(pt) { }
  };

  Queue<Session>     free_sessions;

  struct per_cpu {
    cap_sel ec_service;         // session life-cycle (open, close)
    cap_sel pt_service;         // runs on ec_service

    cap_sel ec_client;          // normal client<->server interaction
    cap_sel pt_flush;           // drain clients

    // Structure only modified in ec_service!
    Session *sessions;

    Session *find_session(cap_sel sm_pseudonym)
    {
      for (Session *c = sessions; c != NULL; c = c->next)
        if (c->sm_pseudonym == sm_pseudonym) return c;
      return NULL;
    }

    void remove_session(Session *s)
    {
      if (sessions == s) {
        sessions = sessions->next;
      } else for (Session *c = sessions; c != NULL; c = c->next) {
          if (c->next == s) {
            c->next = s->next;
            return;
          }
        }
    }

  } _per_cpu[Config::MAX_CPUS];

  Session *new_session(mword func)
  {
    Session *s = free_sessions.dequeue();
    if (s == NULL) {
      Logging::printf("New session object allocated: %p\n", s);
      s = new Session(alloc_cap());
    }

    // Update closure
    s->closure.set(reinterpret_cast<mword>(s), func);
    return s;
  }

public:

  unsigned alloc_crd()
  {
    return Crd(alloc_cap(), 0, DESC_CAP_ALL).value();
  }

  static unsigned flush_func()
  {
    return 0;
  }

  static unsigned client_func_s(unsigned pt_id, Session *s,
                              PerCpuService *tls, Utcb *utcb) REGPARM(2)
  {
    return tls->client_func(pt_id, s);
  }

  unsigned client_func(unsigned pt_id, Session *session)
  {
    // Logging::printf("Client call pt %u session %p tls %p\n",
    //                 pt_id, session, this);

    return EPROTO;
  }

  void garbace_collect(per_cpu &local)
  {
    // This method can only run in ec_service. It modifies the session
    // list while traversing it, which is no problem as there is no
    // concurrency. Remember: Session list modifications only happen
    // in ec_service.

    for (Session *s = local.sessions; s != NULL; s = s->next) {
      if (nova_lookup(Crd(s->sm_pseudonym, 0, DESC_CAP_ALL)).value() == 0) {
        Logging::printf("Garbage collect session %p\n", s);
        close_session(local, s);
      }
    }
  }

  void close_session(per_cpu &local, Session *s)
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
    free_sessions.enqueue(s);
    Logging::printf("Closed %p\n", s);
  }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap)
  {
    unsigned op;
    check1(EPROTO, input.get_word(op));

    per_cpu &local = _per_cpu[mycpu()];
    unsigned res;

    switch (op) {
    case ParentProtocol::TYPE_OPEN: {
      Session *s = local.find_session(input.received_cap());;

      if (s) {
        Logging::printf("REOPEN: %p\n", s);
        goto map_session;
      }

      // Garbage collect everytime (we might want to change this)
      garbace_collect(local);

      s = new_session(reinterpret_cast<mword>(&PerCpuService::client_func_s));
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
      Session *s = local.find_session(input.received_cap());
      if (s == 0) return EPROTO;
      assert(s->sm_pseudonym == input.received_cap());
      close_session(local, s);
      return ENONE;
    }
    };

    return EPROTO;
  }

  NORETURN
  void run(Utcb *utcb, Hip *hip)
  {
    init(hip);
    init_mem(hip);

    console_init("pcpus");
    Logging::printf("Hello.\n");

    Logging::printf("Constructing service...\n");

    const char service_name[] = "/pcpus";
    cap_sel  service_cap = alloc_cap();
    unsigned res;
    mword portal_func = reinterpret_cast<mword>(StaticPortalFunc<PerCpuService>::portal_func);
    mword flush_func = reinterpret_cast<mword>(PerCpuService::flush_func);
    for (phy_cpu_no i = 0; i < hip->cpu_desc_count(); i++) {
      Hip_cpu const &cpu = hip->cpus()[i];
      if (not cpu.enabled()) continue;

      _per_cpu[i].sessions = NULL;

      // Create client EC
      Utcb *utcb_client;
      _per_cpu[i].ec_client = create_ec_helper(this, i, 0, &utcb_client, NULL);
      assert(_per_cpu[i].ec_client != 0);

      _per_cpu[i].pt_flush = alloc_cap();
      res = nova_create_pt(_per_cpu[i].pt_flush, _per_cpu[i].ec_client,
                           flush_func, 0);
      assert(res == NOVA_ESUCCESS);

      // Create service EC
      Utcb *utcb_service;
      _per_cpu[i].ec_service = create_ec_helper(this, i, 0, &utcb_service, NULL);
      assert(_per_cpu[i].ec_service != 0);
      utcb_service->head.crd = alloc_crd();
      utcb_service->head.crd_translate = Crd(0, 31, DESC_CAP_ALL).value();
      
      _per_cpu[i].pt_service = alloc_cap();
      res = nova_create_pt(_per_cpu[i].pt_service, _per_cpu[i].ec_service,
                           portal_func, 0);
      assert(res == NOVA_ESUCCESS);

      // Register service
      res = ParentProtocol::register_service(*utcb, service_name, i,
                                             _per_cpu[i].pt_service, service_cap);
      if (res != ENONE)
        Logging::panic("Registering service on CPU%u failed.\n", i);
    }

    Logging::printf("Service registered.\n");

    // Test
    {
      {
        CpuLocalClient c(*BaseProgram::myutcb(), this, "/s0/pcpus", 0);
      }

      // uint64 start = Cpu::rdtsc();
      // for (unsigned i = 0; i < 0x1000; i++) {
      //   CpuLocalClient c(*BaseProgram::myutcb(), this, "/s0/pcpus", 0);
      // }
      // Logging::printf("open/close cycles %llu\n", (Cpu::rdtsc() - start) / 0x1000);
    }

    Logging::printf("Next...\n");

    {
      // Try to open a session:
      CpuLocalClient c(*BaseProgram::myutcb(), this, "/s0/pcpus", 0);
      // Warmup
      c.call();

      uint64 start = Cpu::rdtsc();
      for (unsigned i = 0; i < 0x1000; i++) {
        res = c.call();
        assert(res == ENONE);
      }
      Logging::printf("call cycles %llu\n", (Cpu::rdtsc() - start) / 0x1000);
    }

    Logging::printf("Done.\n");
    block_forever();
  }

};

ASMFUNCS(PerCpuService, NovaProgram)

// EOF
