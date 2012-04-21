/* -*- Mode: C++ -*-
 * WvTest:
 *   Copyright (C) 2011, 2012 Michal Sojka <sojka@os.inf.tu-dresden.de>
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
#define WVSTART(title) Logging::printf("Testing \"%s\" in %s:%d:\n", title, WvTest::repo_rel_path(__FILE__), __LINE__);
#define WVPASS(cond)   ({ WvTest __t(__FILE__, __LINE__, #cond);            __t.check(cond); })
#define WVNUL(nulerr)  ({ WvTest __t(__FILE__, __LINE__, #nulerr);          __t.check_nulerr(nulerr); })
#define WVNOVA(novaerr)({ WvTest __t(__FILE__, __LINE__, #novaerr);         __t.check_novaerr(novaerr); })
#define WVPASSEQ(a, b) ({ WvTest __t(__FILE__, __LINE__, #a " == " #b);     __t.check_eq((a), (b), true); })
#define WVPASSLT(a, b) ({ WvTest __t(__FILE__, __LINE__, #a " < " #b);      __t.check_lt((a), (b)); })
#define WVPASSGE(a, b) ({ WvTest __t(__FILE__, __LINE__, #a " >= " #b);     __t.check_le((b), (a)); })
#define WVFAIL(cond)   ({ WvTest __t(__FILE__, __LINE__, "NOT(" #cond ")"); !__t.check(!(cond)); })
#define WVFAILEQ(a, b) ({ WvTest __t(__FILE__, __LINE__, #a " != " #b);     __t.check_eq((a), (b), false); })
#define WVPASSNE(a, b) WVFAILEQ(a, b)
#define WVFAILNE(a, b) WVPASSEQ(a, b)

// Performance monitoring
#define WVPERF(value, units)  ({ WvTest __t(__FILE__, __LINE__, "PERF: " #value);  __t.check_perf(value, units); })

// Debugging
#define WV(code)    ({ WvTest __t(__FILE__, __LINE__, #code); __t.check(true); code; })
#define WVSHOW(val) ({ WvTest __t(__FILE__, __LINE__, #val);  __t.show(val); })
#define WVSHOWHEX(val)  ({ WvTest __t(__FILE__, __LINE__, #val);  __t.show_hex(val); })
#define WVPRINTF(fmt, ...)  Logging::printf("! %s:%d " fmt " ok\n", WvTest::repo_rel_path(__FILE__), __LINE__, ##__VA_ARGS__)

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

  struct NovaErr {
    enum ERROR err;
    NovaErr(unsigned char _err) : err(static_cast<enum ERROR>(_err)) {};

    const char *tostr() {
      #define ER(x) case NOVA_##x: return #x
      switch (err) {
      case NOVA_ESUCCESS: return "ok";
	ER(ETIMEOUT);
	ER(EABORT);
	ER(ESYS);
	ER(ECAP);
	ER(EMEM);
	ER(EFTR);
	ER(ECPU);
	ER(EDEV);
      }
      #undef ER
      char *ret = new char[30]; // XXX memory leak
      Vprintf::snprintf(ret, 30, "ERR:0x%x", err);
      return ret;
    }
  };

  const char *resultstr(bool result)
    {
//       if (!result)
// 	tests_failed++;
      return result ? "ok" : "FAILED";
    }

  const char *resultstr(char *result)
    {
//       if (!strcmp(result, "ok"))
// 	tests_failed++;
      return result;
    }

  const char *resultstr(NulErr result)
    {
//       if (result.err != ENONE)
// 	tests_failed++;
      return result.tostr();
    }

  const char *resultstr(NovaErr result)
    {
//       if (result.err != ENONE)
// 	tests_failed++;
      return result.tostr();
    }

  void save_info(const char *_file, int _line, const char *_condstr)
    { file = repo_rel_path(_file); condstr = _condstr; line = _line; }

#if WVTEST_PRINT_INFO_BEFORE
  void print_info()
    { Logging::printf("! %s:%d %s ", file, line, condstr); }

  template <typename T>
  void print_result(T result, const char* suffix="", const char *sb="", const char *se="")
    { Logging::printf("%s%s%s %s\n", sb, suffix, se, resultstr(result)); }
#else
  template <typename T>
  void print_result(T result, const char* suffix="", const char *sb="", const char *se="")
    {
      Logging::printf("! %s:%d %s %s%s%s %s\n", file, line, condstr, sb, suffix, se, resultstr(result));
      //tests_run++;
    }
#endif

  static void print_failed_cmp(const char *op, const char *a, const char *b)
    { Logging::printf("wvtest comparison '%s' %s '%s' FAILED\n", a, op, b); }

  static void print_failed_cmp(const char *op, unsigned a, unsigned b)
    { Logging::printf("wvtest comparison %d == 0x%x %s %d == 0x%x FAILED\n",
		      a, a, op, b, b); }

  static void print_failed_cmp(const char *op, unsigned long a, unsigned long b)
    { Logging::printf("wvtest comparison %ld == 0x%lx %s %ld == 0x%lx FAILED\n",
		      a, a, op, b, b); }

  static void print_failed_cmp(const char *op, unsigned long long a, unsigned long long b)
    { Logging::printf("wvtest comparison %llu == 0x%llx %s %llu == 0x%llx FAILED\n",
		      a, a, op, b, b); }

  static void print_failed_cmp(const char *op, int a, int b)
    { Logging::printf("wvtest comparison %d == 0x%x %s %d == 0x%x FAILED\n",
		      a, a, op, b, b); }

  static void stringify(char *buf, unsigned size, unsigned long long val)
  { Vprintf::snprintf(buf, size, "%llu", val); }

  static void stringify(char *buf, unsigned size, unsigned long val)
  { Vprintf::snprintf(buf, size, "%lu", val); }

  static void stringify(char *buf, unsigned size, unsigned val)
  {Vprintf::snprintf(buf, size, "%u", val);}

  static void stringify(char *buf, unsigned size, int val)
  {Vprintf::snprintf(buf, size, "%d", val);}

  static void stringify(char *buf, unsigned size, Crd crd) {
    switch (crd.value() & 0x3) {
    case 0:
      Vprintf::snprintf(buf, size, "CRD(0)"); return;
    case DESC_TYPE_MEM: {
      char r = crd.value() & 0x04 ? 'r' : '-';
      char w = crd.value() & 0x08 ? 'w' : '-';
      char x = crd.value() & 0x10 ? 'x' : '-';
      Vprintf::snprintf(buf, size, "CRD(mem, 0x%x+0x%x, %c%c%c)", crd.base(), crd.size(), r, w, x);
      return;
    }
    case DESC_TYPE_IO:
      Vprintf::snprintf(buf, size, "CRD(io, 0x%x, 2^%d)", crd.base(), crd.order());
      return;
    case DESC_TYPE_CAP:
      Vprintf::snprintf(buf, size, "CRD(obj, 0x%x, 2^%d, 0x%x)", crd.cap(), crd.order(), crd.value() >> 2 & 0x1f);
      return;
    }
  }

  static void stringifyx(char *buf, unsigned size, unsigned long long val)
  { Vprintf::snprintf(buf, size, "0x%llx", val); }

  static void stringifyx(char *buf, unsigned size, unsigned long val)
  { Vprintf::snprintf(buf, size, "0x%lx", val); }

  static void stringifyx(char *buf, unsigned size, unsigned val)
  {Vprintf::snprintf(buf, size, "0x%x", val);}

  static void stringifyx(char *buf, unsigned size, int val)
  {Vprintf::snprintf(buf, size, "0x%x", val);}

  static void stringifyx(char *buf, unsigned size, void *val)
  {Vprintf::snprintf(buf, size, "%p", val);}

public:
  //static unsigned tests_failed, tests_run;

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

  unsigned check_nulerr(unsigned nulerr) {
    print_result(NulErr(nulerr));
    return nulerr;
  }

  unsigned check_novaerr(unsigned novaerr) {
    print_result(NovaErr(novaerr));
    return novaerr;
  }

  bool check_eq(const char *a, const char *b, bool expect_equal)
    {
      bool result = !!strcmp(a, b) ^ expect_equal;
      if (!result) print_failed_cmp(expect_equal ? "==" : "!=", a, b);
      check(result);
      return result;
    }
  bool check_eq(      char *a,       char *b, bool expect_equal) { return check_eq(static_cast<const char*>(a), static_cast<const char*>(b), expect_equal); }
  bool check_eq(const char *a,       char *b, bool expect_equal) { return check_eq(static_cast<const char*>(a), static_cast<const char*>(b), expect_equal); }
  bool check_eq(      char *a, const char *b, bool expect_equal) { return check_eq(static_cast<const char*>(a), static_cast<const char*>(b), expect_equal); }


  template <typename T>
  bool check_eq(T a, T b, bool expect_equal)
    {
      bool result = (a == b ^ !expect_equal);
      if (!result) print_failed_cmp(expect_equal ? "==" : "!=", a, b);
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
  bool check_le(T a, T b)
    {
      bool result = (a <= b);
      if (!result) print_failed_cmp("<=", a, b);
      check(result);
      return result;
    }

  template <typename T>
  T check_perf(T val, const char *units)
  {
    char valstr[20+strlen(units)];
    stringify(valstr, sizeof(valstr), val);
    print_result(true, " ", valstr, units);
    return val;
  }

  const char *show(const char *val)
  {
    print_result(true, val, "= \"", "\"");
    return val;
  }

  char *show(char *val)
  {
    print_result(true, val, "= \"", "\"");
    return val;
  }

  template <typename T>
  T show(T val)
  {
    char valstr[40];
    stringify(valstr, sizeof(valstr), val);
    print_result(true, valstr, "= ");
    return val;
  }

  template <typename T>
  T show_hex(T val)
  {
    char valstr[40];
    stringifyx(valstr, sizeof(valstr), val);
    print_result(true, valstr, "= ");
    return val;
  }

  /**
   * Custom exit function. Reports exit as a failure because it is
   * typically called via assert() or panic(). Use this as
   * ASMFUNCS(..., WvTest).
   */
  __attribute__ ((noreturn))
  static void exit(const char *msg = 0)
  {
    if (msg && strcmp(msg, "run returned"))
      Logging::printf("! %s() - %s FAILED\n", __func__, msg);

    // Signal an event to parent
    if (ParentProtocol::signal(*BaseProgram::myutcb(), 0))
      Logging::printf("! wvtest.h:%d ParentProtocol::signal(0)) FAILED\n", __LINE__);

    // We want to block forever, but we do not have cap allocator to
    // allocate a semaphore. Therefore, we revoke our EC and use this
    // cap for the semaphore. The EC will (unfortnately) still exist
    // after the revocation because there is an SC managed by
    // admission service that refers to it.
    unsigned res;
    nova_revoke_self(Crd(ParentProtocol::CAP_CHILD_EC, 0, DESC_CAP_ALL));
    unsigned block = ParentProtocol::CAP_CHILD_EC;
    res = nova_create_sm(block);
    assert(res == 0);
    while (1) { res = nova_semdown(block); assert(res == 0); }
  }
};

//unsigned WvTest::tests_failed, WvTest::tests_run;


#endif // __WVTEST_H
