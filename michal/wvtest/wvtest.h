/* -*- Mode: C++ -*-
 * WvTest:
 *   Copyright (C) 2011 Michal Sojka <sojka@os.inf.tu-dresden.de>
 *   Copyright (C) 1997-2009 Net Integration Technologies, Inc.
 *       Licensed under the GNU Library General Public License, version 2.
 *       See the included file named LICENSE for license information.
 */
#ifndef __WVTEST_H
#define __WVTEST_H

#include <nul/program.h>
#include "sigma0/console.h"

// Whether to print info before execution of the test. Zero means
// print info after execution together with results.
#define WVTEST_PRINT_INFO_BEFORE 0

#define WVPASS(cond)   ({ WvTest::start(__FILE__, __LINE__, #cond);            WvTest::check(cond); })
#define WVNUL(nulerr)  ({ WvTest::start(__FILE__, __LINE__, #nulerr);          WvTest::check_nulerr(nulerr); })
#define WVPASSEQ(a, b) ({ WvTest::start(__FILE__, __LINE__, #a " == " #b);     WvTest::check_eq((a), (b), true); })
#define WVPASSLT(a, b) ({ WvTest::start(__FILE__, __LINE__, #a " < " #b);      WvTest::check_lt((a), (b)); })
#define WVFAIL(cond)   ({ WvTest::start(__FILE__, __LINE__, "NOT(" #cond ")"); !WvTest::check(!(cond)); })
#define WVFAILEQ(a, b) ({ WvTest::start(__FILE__, __LINE__, #a " != " #b);     WvTest::check_eq((a), (b), false); })
#define WVPASSNE(a, b) WVFAILEQ(a, b)
#define WVFAILNE(a, b) WVPASSEQ(a, b)

class WvTest
{
protected:
  const char *file, *condstr;
  int line;
  unsigned tests_failed, tests_run;

  struct NulErr {
    unsigned err;
    NulErr(unsigned _err) : err(_err) {};

    const char *tostr() {
      #define ER(x) case x: return #x
      switch (err) {
      case ENONE: return "ok";
	ER(EPROTO);
	ER(EPERM);
	ER(ERETRY);
	ER(EABORT);
	ER(ERESOURCE);
	ER(EEXISTS);
	ER(ELASTGLOBAL);
      default:
	char *ret = new char[30]; // XXX memory leak
	Vprintf::snprintf(ret, 30, "ERR:0x%x", err);
	return ret;
      }
    }
    #undef ER
  };
protected:
  const char *resultstr(bool result)
    {
      if (!result)
	tests_failed++;
      return result ? "ok" : "FAILED";
    }

  const char *resultstr(char *result)
    {
      if (!strcmp(result, "ok")) 
	tests_failed++;
      return result;
    }

  const char *resultstr(NulErr result)
    {
      if (result.err != ENONE)
	tests_failed++;
      return result.tostr();
    }

  void save_info(const char *_file, int _line, const char *_condstr)
    { file = _file; condstr = _condstr; line = _line; }

#if WVTEST_PRINT_INFO_BEFORE  
  void print_info()
    { Logging::printf("! %s:%d %s ", file, line, condstr); }

  template <typename T>
  void print_result(T result, const char* suffix="")
    { Logging::printf("%s %s\n", suffix, resultstr(result)); }
#else
  template <typename T>
  void print_result(T result, const char* suffix="")
    {
      Logging::printf("! %s:%d %s %s %s\n", file, line, condstr, suffix, resultstr(result));
      tests_run++;
    }
#endif
  void start(const char *file, int line, const char *condstr) {
    save_info(file, line, condstr);
#if WVTEST_PRINT_INFO_BEFORE    
    // If we are sure that nothing is printed during the "check", we can
    // print the info here, and the result after the "check" finishes.
    print_info(true, file, line, condstr, 0);
#endif
  }

  bool check(bool cond, const char* suffix = "") {
    print_result(cond, suffix);
    return cond;
  }

  bool check_nulerr(unsigned nulerr) {
    print_result(NulErr(nulerr));
    return nulerr == ENONE;
  }

  static void print_failed_cmp(const char *op, const char *a, const char *b)
    { Logging::printf("wvtest comparison %s %s %s FAILED\n", a, op, b); }

  static void print_failed_cmp(const char *op, unsigned a, unsigned b)
    { Logging::printf("wvtest comparison %d == 0x%x %s %d == 0x%x FAILED\n",
		      a, a, op, b, b); }

  static void print_failed_cmp(const char *op, int a, int b)
    { Logging::printf("wvtest comparison %d == 0x%x %s %d == 0x%x FAILED\n",
		      a, a, op, b, b); }


  template <typename T>
  bool check_eq(T a, T b, bool expect_equal)
    {
      bool result = (a == b ^ !expect_equal);
      if (!result) print_failed_cmp("==", a, b);
      check(result);
      return result;
    }  

  template <typename T>
  bool check_lt(T a, T b)
    {
      bool result = (a < b);
      if (!result) print_failed_cmp("<", a, b);
      check(result);
      return result;
    }  
};

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
    do_exit("run returned");
  }
};

#endif // __WVTEST_H
