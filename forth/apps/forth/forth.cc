#include <ficl.h>
#include "sigma0/console.h"
#include <nul/program.h>

#include <stdio.h>


extern "C" void *realloc(void *ptr, size_t size)
{
  assert(false);
  return NULL;
}

extern "C" int
putchar(int c)
{
  Logging::printf("%c", c);
  return c;
}

static void rdtsc(FICL_VM *pVM)
{
  uint32_t hi, lo;
  asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
  PUSHUNS(hi); PUSHUNS(lo);
}

static void qwMinus(FICL_VM *pVM)
{
  uint64_t a = (uint64_t)POPUNS();
  a |= (uint64_t)POPUNS() << 32;

  uint64_t b = (uint64_t)POPUNS();
  b |= (uint64_t)POPUNS() << 32;

  uint64_t res = b - a;
  PUSHUNS(res >> 32); PUSHUNS((uint32_t)res);
}

static unsigned fibCr(unsigned i)
{
  if (i<2)
    return 2;
  else
    return fibCr(i-1) + fibCr(i-2);
}

static unsigned fibCi(unsigned n)
{
  if (n<2)
    return 2;
  else {
    unsigned last1 = 1;
    unsigned last2 = 1;
    for (unsigned i = 1; i < n; i++) {
      unsigned r = last1 + last2;
      last1 = last2;
      last2 = r;
    }
    return last2;
  }
}

static void FfibCr(FICL_VM *pVM) { PUSHUNS(fibCr(POPUNS())); }
static void FfibCi(FICL_VM *pVM) { PUSHUNS(fibCi(POPUNS())); }

class Forth : public ProgramConsole
{
  unsigned _cap_free;
public:
  int run(Utcb *utcb, Hip *hip)
  {
    console_init("Forth");
    printf("Forth booting...\n");

    FICL_SYSTEM *pSys = ficlInitSystem(10000);
    dictAppendWord(pSys->dp, "rdtsc",    rdtsc,     FW_DEFAULT); // ( -- hi lo )
    dictAppendWord(pSys->dp, "qw-",      qwMinus,   FW_DEFAULT); // ( bhi blo ahi alo -- rhi rlo )
    dictAppendWord(pSys->dp, "fibcr",    FfibCr,    FW_DEFAULT);
    dictAppendWord(pSys->dp, "fibci",    FfibCi,    FW_DEFAULT);
    
    FICL_VM *pVM = ficlNewVM(pSys);

    ficlEvaluate(pVM, ".ver .( " __DATE__ " ) cr cr quit");
    ficlEvaluate(pVM,
		 ": fib { n } n 2 < if n else n 1 - recurse n 2 - recurse + then ; "
		 ": fibi { n } n 2 < if n else 1 1 n 1 do swap over + loop swap drop then ; "
		 );

    // B0rken :-/
    //ficlEvaluate(pVM, "see fib");

    // Slow
    ficlEvaluate(pVM, ".( fib 20 = ) 20 fib . cr");
    // Slightly faster
    printf("fib 30 = ");
    PUSHUNS(30);
    ficlEvaluate(pVM, "fib ");
    printf("%u\n", POPUNS());

    // Speed test
    ficlEvaluate(pVM, 
		 ": rdtsc-empty rdtsc rdtsc 2swap qw- ; "
		 ": fib-rdtsc { n -- chi clo } rdtsc n fib drop rdtsc 2swap qw- ; "
		 ": fibcr-rdtsc { n -- chi clo } rdtsc n fibcr drop rdtsc 2swap qw- ; "
		 ": fibi-rdtsc { n -- chi clo } rdtsc n fibi drop rdtsc 2swap qw- ; "
		 ": fibci-rdtsc { n -- chi clo } rdtsc n fibci drop rdtsc 2swap qw- ; "
		 "rdtsc-empty .( EMPTY ) . drop .( cycles ) cr "
		 "10 fib-rdtsc .( FIB  10 ) . drop .( cycles ) cr "
		 "10 fibcr-rdtsc .( FIBCR  10 ) . drop .( cycles ) cr "
		 "100 fibi-rdtsc .( FIBI 100 ) . drop .( cycles ) cr "
		 "1000 fibi-rdtsc .( FIBI 1000 ) . drop .( cycles ) cr "
		 "100 fibci-rdtsc .( FIBCI 100 ) . drop .( cycles ) cr "
		 "1000 fibci-rdtsc .( FIBCI 1000 ) . drop .( cycles ) cr "

		 );

    // Pass our cap allocation int to Forth.
    _cap_free = hip->cfg_exc + hip->cfg_gsi + 3;
    PUSHPTR(&_cap_free);
    ficlEvaluate(pVM, "capptr ! ");

    // Try some semaphore fun.
    ficlEvaluate(pVM,
		 "variable sem alloccap sem ! "
		 " : checksys dup 0= if s\" SUCCESS\" type drop else s\" ERROR \" type . then ; "
		 ".( create sm ) 0 sem @ createsm checksys cr "
		 ".( up ) sem @ semup checksys cr "
		 ".( up ) sem @ semup checksys cr "
		 ".( down ) sem @ semdown checksys cr "
		 ".( down ) sem @ semdown checksys cr "
		 ".( down ) sem @ semdown checksys cr "
		 );

    printf("\nForth done?\n");
    return 0;
  }
};

ASMFUNCS(Forth, NovaProgram);
