// -*- Mode: C++ -*-

#include <ficl.h>
#include <sys/syscalls.h>
#include <stdio.h>

static unsigned *free_cap;

static void CapPtr(FICL_VM *pVM)
{
  stackPushPtr(pVM->pStack, &free_cap);
}

static void allocCap(FICL_VM *pVM)
{
  stackPushUNS(pVM->pStack, (*free_cap)++);
}

static void createSm(FICL_VM *pVM)
{
  unsigned cap  = stackPopUNS(pVM->pStack);
  unsigned init = stackPopUNS(pVM->pStack);

  stackPushUNS(pVM->pStack, create_sm(cap, init));
}

static void semUp(FICL_VM *pVM)
{
  unsigned cap = stackPopUNS(pVM->pStack);
  stackPushUNS(pVM->pStack, semup(cap));
}

static void semDown(FICL_VM *pVM)
{
  unsigned cap = stackPopUNS(pVM->pStack);
  stackPushUNS(pVM->pStack, semdown(cap));
}

static void rdtsc(FICL_VM *pVM)
{
  uint32_t hi, lo;
  asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
  PUSHUNS(hi); PUSHUNS(lo);
}

extern "C"
void ficlCompileNova(FICL_SYSTEM *pSys)
{
  FICL_DICT *dp = pSys->dp;
  assert (dp);

  dictAppendWord(dp, "capptr",   CapPtr,    FW_DEFAULT);
  dictAppendWord(dp, "alloccap", allocCap,  FW_DEFAULT);  
  dictAppendWord(dp, "createsm", createSm,  FW_DEFAULT); // ( init capidx -- success )
  dictAppendWord(dp, "semup",    semUp,     FW_DEFAULT); // ( capidx -- success )
  dictAppendWord(dp, "semdown",  semDown,   FW_DEFAULT); // ( capidx -- success )
  
  dictAppendWord(dp, "rdtsc",    rdtsc,     FW_DEFAULT); // ( -- hi lo )

}
