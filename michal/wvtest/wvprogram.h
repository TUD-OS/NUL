#ifndef WVPROGRAM_H
#define WVPROGRAM_H

#include <wvtest.h>

#include <nul/program.h>
#include "sigma0/console.h"

class WvProgram : public NovaProgram, public ProgramConsole, public WvTest
{
public:

  void test_init(Utcb *utcb, Hip *hip) {
    init(hip);
    init_mem(hip);
    console_init("wvtest", new Semaphore(alloc_cap(), true));

    // Connect to the trace buffer
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_count()));

    //Logging::printf("\nTesting \"%s\" in %s:\n", descr, file);
  }

  void test_done() {
    WVPASS(tests_run > 0);

    Logging::printf("WvTest: %d test%s, %d failure%s.\n",
		    tests_run, tests_run==1 ? "" : "s",
		    tests_failed, tests_failed==1 ? "": "s");
    Logging::printf("\nFinished testing in %s.\n", file);
    //return tests_failed != 0;
  }

  virtual void wvrun(Utcb *utcb, Hip *hip) = 0;

  void run(Utcb *utcb, Hip *hip) {
    test_init(utcb, hip);
    wvrun(utcb, hip);
    test_done();
    do_exit(0);
  }
};

#endif
