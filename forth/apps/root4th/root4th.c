/* -*- Mode: C -*- */

#include <string.h>
#include <ficl.h>

int putchar(int c) { return c; }

__attribute__ ((noreturn))
void __exit(unsigned long status) { *(volatile char *)(0xF00 | (status & 0xFF)) = 0; while (1) { }}

void free(void *p) { }

__attribute__ ((noreturn))
void *realloc(void *p, size_t size) { __exit(1); }

extern char _early_heap_start[];
extern char _early_heap_end[];
char *early_heap_alloc = _early_heap_start;

void *malloc(size_t s)
{
  void *p = early_heap_alloc;
  early_heap_alloc += s;
  if (early_heap_alloc >= _early_heap_end) __exit(2);
  return p;
}

extern char _heap_start[];
extern char _utcb_start[];

static void novaSyscall(FICL_VM *pVM) /* ( w0 w1 w2 w3 w4 -- success ) */
{
  unsigned w4 = POPUNS();
  unsigned w3 = POPUNS();
  unsigned w2 = POPUNS();
  unsigned w1 = POPUNS();
  unsigned w0 = POPUNS();

  asm volatile ("push %%ebp;"
		"mov %%ecx, %%ebp;"
		"mov %%esp, %%ecx;"
		"mov $1f, %%edx;"
		"sysenter;"
		"1: ;"
		"pop %%ebp;"
		: "+a" (w0), "+c"(w4)
		: "D" (w1), "S" (w2), "b"(w3)
		: "edx", "memory");

  PUSHUNS(w0 & 0xFF);
}

extern char _ec_template[];
extern char _ec_template_end[];
static void ecTrampoline(FICL_VM *pVM) /* ( word -- ) */
{
  size_t temp_size = _ec_template_end - _ec_template;
  unsigned char *tramp = malloc(temp_size);
  memcpy(tramp, _ec_template, temp_size);
  *(uint32_t *)(tramp+0*5+1) = (uint32_t)ficlNewVM(pVM->pSys);
  *(uint32_t *)(tramp+1*5+1) = (uint32_t)POPPTR();

  PUSHPTR(tramp);
}

uint32_t execPtFn(FICL_VM *pVM, FICL_WORD *pWord)
{
  ficlExecXT(pVM, pWord);
#if FICL_ROBUST > 1
  if (stackDepth(pVM->pStack) != 1)
    __exit(4);
#endif
  return POPUNS();
}

static char forth_code[] = {
#include "main.inc"
  0 };

__attribute__((noreturn)) void start(void *hip)
{
  FICL_SYSTEM *pSys = ficlInitSystem(10000);
  dictAppendWord(pSys->dp, "nova-syscall",  novaSyscall,  FW_DEFAULT);
  dictAppendWord(pSys->dp, "ec-trampoline", ecTrampoline, FW_DEFAULT);
  FICL_VM *pVM = ficlNewVM(pSys);

  PUSHPTR((char *)hip - 0x1000); /* UTCB */
  PUSHPTR(hip);
  PUSHPTR(_heap_start);
  PUSHPTR(_utcb_start);
  ficlEvaluate(pVM, 
               "constant utcb-start "
               "constant heap-start "
               "constant hip "
               "constant main-utcb "
               );

  ficlEvaluate(pVM, forth_code);
  __exit(3);
}

/* EOF */
