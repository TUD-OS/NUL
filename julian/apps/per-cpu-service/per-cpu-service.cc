#include "closure.h"
#include "client.h"
#include "service.h"
#include <wvtest.h>

class PerCpuService : public ServiceProgram
{
protected:
  struct Session : public BaseSession {
    explicit Session(cap_sel pt) : BaseSession(pt) { }
  };

  BaseSession *new_session()
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

    _console_data.log = new LogProtocol(NovaProgram::alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_count()));

    // Test
    {
      {
        Client c(*utcb, this, "s0/pcpus", 0);
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
      Client c(*utcb, this, "s0/pcpus", 0);
      // Warmup
      c.call();

      uint64 start = Cpu::rdtsc();
      for (unsigned i = 0; i < 0x1000; i++) {
        res = c.call();
        assert(res == ENONE);
      }
      Logging::printf("call cycles %llu\n", (Cpu::rdtsc() - start) / 0x1000);
    }

    WV("Done");
    WvTest::exit(0);
    block_forever();
  }

};

ASMFUNCS(PerCpuService, WvTest)

// EOF
