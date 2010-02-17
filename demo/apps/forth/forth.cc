#include <ficl.h>
#include "sigma0/console.h"

#include <stdio.h>

extern "C" int
putchar(int c)
{
  Logging::printf("%c", c);
  return c;
}

class Forth : public ProgramConsole
{
  unsigned _cap_free;
public:
  int run(Hip *hip)
  {
    console_init("Forth");
    printf("Forth booting...\n");

    FICL_SYSTEM *pSys = ficlInitSystem(10000);
    // XXX Add new words here.
    FICL_VM *pVM = ficlNewVM(pSys);

    ficlEvaluate(pVM, ".ver .( " __DATE__ " ) cr cr quit");
    ficlEvaluate(pVM, ": fib { n } n 2 < if n else n 1 - recurse n 2 - recurse + then ;");

    // B0rken :-/
    //ficlEvaluate(pVM, "see fib");

    // Slow
    ficlEvaluate(pVM, ".( fib 20 = ) 20 fib . cr");
    // Slightly faster
    printf("fib 30 = ");
    PUSHUNS(30);
    ficlEvaluate(pVM, "fib ");
    printf("%u\n", POPUNS());

    // Pass our cap allocation int to Forth.
    _cap_free = hip->cfg_exc + hip->cfg_gsi + 3;
    PUSHPTR(&_cap_free);
    ficlEvaluate(pVM, "16 base ! "
		 "capptr ! ");

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
