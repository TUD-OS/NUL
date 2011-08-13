/* -*- Mode: C++ -*-
 * WvTest:
 *   Copyright (C) 2011 Michal Sojka <sojka@os.inf.tu-dresden.de>
 *   Copyright (C) 1997-2009 Net Integration Technologies, Inc.
 *       Licensed under the GNU Library General Public License, version 2.
 *       See the included file named LICENSE for license information.
 */
#ifndef __WVTEST_H
#define __WVTEST_H

#include <nul/error.h>
#include <nul/parent.h>
#include <service/vprintf.h>
#include <service/string.h>
#include <service/logging.h>
#include <nul/baseprogram.h>

// Whether to print info before execution of the test. Zero means
// print info after execution together with results.
#define WVTEST_PRINT_INFO_BEFORE 0

// Standard WVTEST API
#define WVPASS(cond)   ({ WvTest t(__FILE__, __LINE__, #cond);            t.check(cond); })
#define WVNUL(nulerr)  ({ WvTest t(__FILE__, __LINE__, #nulerr);          t.check_nulerr(nulerr); })
#define WVPASSEQ(a, b) ({ WvTest t(__FILE__, __LINE__, #a " == " #b);     t.check_eq((a), (b), true); })
#define WVPASSLT(a, b) ({ WvTest t(__FILE__, __LINE__, #a " < " #b);      t.check_lt((a), (b)); })
#define WVFAIL(cond)   ({ WvTest t(__FILE__, __LINE__, "NOT(" #cond ")"); !t.check(!(cond)); })
#define WVFAILEQ(a, b) ({ WvTest t(__FILE__, __LINE__, #a " != " #b);     t.check_eq((a), (b), false); })
#define WVPASSNE(a, b) WVFAILEQ(a, b)
#define WVFAILNE(a, b) WVPASSEQ(a, b)

// Performance monitoring
#define WVPERF(value)  ({ WvTest t(__FILE__, __LINE__, "PERF: " #value);  t.check_perf(value); })

// Debugging
#define WV(code)  ({ WvTest t(__FILE__, __LINE__, #code);   t.check(true); code })
#define WVDEC(val)  ({ WvTest t(__FILE__, __LINE__, #val);  t.show_dec(val); })
#define WVHEX(val)  ({ WvTest t(__FILE__, __LINE__, #val);  t.show_hex(val); })

class WvTest
{
  const char *file, *condstr;
  int line;

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

  static const char *repo_rel_path(const char *filename)
  {
    const char wvtest_h[] = __FILE__;
    const char name_in_repo[] = "michal/include/wvtest.h";
    unsigned rel_idx = sizeof(wvtest_h) - sizeof(name_in_repo);

    const char *p1 = wvtest_h + rel_idx;
    const char *p2 = name_in_repo;
    while (*p1 && *p2)
      if (*p1++ != *p2++) // Unexpected location of this file
	return filename;  // Return absolute path

    for (unsigned i=0; i < rel_idx; i++)
      if (filename[i] != wvtest_h[i])
	rel_idx = 0;
    // Return repo-relative path if the 'filename' is in repo.
    return filename + rel_idx;
  }

  void save_info(const char *_file, int _line, const char *_condstr)
    { file = repo_rel_path(_file); condstr = _condstr; line = _line; }

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

  static void print_failed_cmp(const char *op, const char *a, const char *b)
    { Logging::printf("wvtest comparison %s %s %s FAILED\n", a, op, b); }

  static void print_failed_cmp(const char *op, unsigned a, unsigned b)
    { Logging::printf("wvtest comparison %d == 0x%x %s %d == 0x%x FAILED\n",
		      a, a, op, b, b); }

  static void print_failed_cmp(const char *op, int a, int b)
    { Logging::printf("wvtest comparison %d == 0x%x %s %d == 0x%x FAILED\n",
		      a, a, op, b, b); }

  static void stringify(char *buf, unsigned size, unsigned long long val, const char *prefix="")
  { Vprintf::snprintf(buf, size, "%s%llu", prefix, val); }

  static void stringify(char *buf, unsigned size, unsigned val, const char *prefix="")
  {Vprintf::snprintf(buf, size, "%s%u", prefix, val);}

  static void stringify(char *buf, unsigned size, int val, const char *prefix="")
  {Vprintf::snprintf(buf, size, "%s%d", prefix, val);}

  static void stringifyx(char *buf, unsigned size, unsigned long long val, const char *prefix="")
  { Vprintf::snprintf(buf, size, "%s0x%llx", prefix, val); }

  static void stringifyx(char *buf, unsigned size, unsigned val, const char *prefix="")
  {Vprintf::snprintf(buf, size, "%s0x%x", prefix, val);}

  static void stringifyx(char *buf, unsigned size, int val, const char *prefix="")
  {Vprintf::snprintf(buf, size, "%s0x%x", prefix, val);}

public:
  static unsigned tests_failed, tests_run;

  WvTest(const char *file, int line, const char *condstr) {
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

  template <typename T>
  T check_perf(T val)
  {
    char valstr[20];
    stringify(valstr, sizeof(valstr), val);
    print_result(true, valstr);
    return val;
  }

  template <typename T>
  T show_dec(T val)
  {
    char valstr[20];
    stringify(valstr, sizeof(valstr), val, "= ");
    print_result(true, valstr);
    return val;
  }

  template <typename T>
  T show_hex(T val)
  {
    char valstr[20];
    stringifyx(valstr, sizeof(valstr), val, "= ");
    print_result(true, valstr);
    return val;
  }

  /**
   * Custom exit function. Reports exit as a failure because it is
   * typically called via assert() or panic(). Use this as
   * ASMFUNCS(..., WvTest).
   */
  static void exit(const char *msg)
  {
    if (msg)
      Logging::printf("! %s() - %s FAILED\n", __func__, msg);

    // Signal an event to parent
    if (ParentProtocol::signal(*BaseProgram::myutcb(), 0))
      Logging::printf("! wvtest.h:%d ParentProtocol::signal(0)) FAILED\n", __LINE__);
  }
};

unsigned WvTest::tests_failed, WvTest::tests_run;


#endif // __WVTEST_H
