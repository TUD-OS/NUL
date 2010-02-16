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

    printf("\nForth done.\n");
    return 0;
  }
};

ASMFUNCS(Forth, NovaProgram);
