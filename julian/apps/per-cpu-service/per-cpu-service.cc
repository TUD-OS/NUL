#include "closure.h"
#include "client.h"
#include "service.h"

class PerCpuService : public ServiceProgram
{
protected:
  struct Session : public BaseSession {
    explicit Session(cap_sel pt) : BaseSession(pt) { }
  };

  Session *new_session()
  {
    mword func = reinterpret_cast<mword>(&PerCpuService::client_func_s);
    Session *s = static_cast<Session *>(_free_sessions.dequeue());
    if (s == NULL) {
      s = new Session(alloc_cap());
      Logging::printf("New session object allocated: %p\n", s);
    }

    // Update closure
    s->closure.set(reinterpret_cast<mword>(s), func);
    return s;
  }

  static unsigned client_func_s(unsigned pt_id, Session *s,
                                PerCpuService *tls, Utcb *utcb) REGPARM(2)
  {
    return tls->client_func(pt_id, s);
  }

  unsigned client_func(unsigned pt_id, Session *session)
  {
    return ENONE;
  }

public:

  PerCpuService()
  {
    Logging::printf("Hello.\n");
    register_service("/pcpus");
  };

  NORETURN
  void run(Utcb *utcb, Hip *hip)
  {
    unsigned res;

    // Test
    {
      {
        CpuLocalClient c(*utcb, this, "/s0/pcpus", 0);
        res = c.call();
        assert(res == ENONE);
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
      CpuLocalClient c(*utcb, this, "/s0/pcpus", 0);
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
