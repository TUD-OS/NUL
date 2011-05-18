#include <nul/program.h>
#include <sigma0/console.h>

#include <nul/parent.h>
#include <nul/generic_service.h>
#include <nul/capalloc.h>

#include "closure.h"
#include "client.h"

class PerCpuService : public NovaProgram, public ProgramConsole
{
  const static unsigned MAX_PER_CPU_SESSIONS = 0x1000;
  
  struct Session {
    Session * volatile next;

    phy_cpu_no cpu;
    cap_sel    pt_identity;
    cap_sel    pt;
    Closure    closure;
  };

  Session * volatile free_sessions;

  struct per_cpu {
    cap_sel ec_service;         // session life-cycle (open, close)
    cap_sel pt_service;         // runs on ec_service

    cap_sel ec_client;          // normal client<->server interaction
    cap_sel pt_flush;           // drain clients

    // Structure only modified in ec_service!
    Session *sessions;

    Session *find_session(cap_sel pt_identity)
    {
      for (Session *c = sessions; c != NULL; c = c->next)
        if (c->pt_identity == pt_identity) return c;
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

  Session *allocate_session_object(mword func)
  {
  again:
    Session *s = free_sessions;
    if (s == NULL) goto more_memory;
    if (not __sync_bool_compare_and_swap(&free_sessions, s, s->next))
      goto again;
    return s;

  more_memory:
    s = new Session;
    Logging::printf("New session object: %p\n", s);
    s->pt = alloc_cap();
    s->closure.set(reinterpret_cast<mword>(s), func);
    return s;
  }

  void free_session_object(Session *s)
  {
    Session *head;
    do {
      head    = free_sessions;
      s->next = head;
    } while (not __sync_bool_compare_and_swap(&free_sessions, head, s));
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
        goto map_session;
      }

      // New session
      s = allocate_session_object(reinterpret_cast<mword>(&PerCpuService::client_func_s));
      // pt and closure are already set in s

      res = nova_create_pt(s->pt, local.ec_client,
                                    s->closure.value(), 0);
      assert(res == NOVA_ESUCCESS);

      // No synchronization needed. Only modified in this EC
      s->pt_identity = input.received_cap();
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
      assert(s->pt_identity == input.received_cap());
      nova_revoke(Crd(s->pt, 0, DESC_CAP_ALL), true);
      // Ensure that no one holds a pointer to the session object
      res = nova_call(local.pt_flush);
      assert(res == NOVA_ESUCCESS);
      local.remove_session(s);
      free_session_object(s);
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
        NewClient c(*BaseProgram::myutcb(), this, "/s0/pcpus", 0);
      }

      uint64 start = Cpu::rdtsc();
      for (unsigned i = 0; i < 0x1000; i++) {
        NewClient c(*BaseProgram::myutcb(), this, "/s0/pcpus", 0);
      }
      Logging::printf("open/close cycles %llu\n", (Cpu::rdtsc() - start) / 0x1000);
    }


    {
      // Try to open a session:
      NewClient c(*BaseProgram::myutcb(), this, "/s0/pcpus", 0);
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
