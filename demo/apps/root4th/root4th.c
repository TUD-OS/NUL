/* -*- Mode: C -*- */

#include <ficl.h>

int putchar(int c) { return c; }

void __exit(unsigned long status) { *(volatile char *)(0xF00 | (status & 0xFF)) = 0; while (1) { }}

void free(void *p) { }
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

__attribute__((noreturn)) void start(void *hip)
{
  FICL_SYSTEM *pSys = ficlInitSystem(10000);
  FICL_VM *pVM = ficlNewVM(pSys);

  PUSHPTR(hip);
  PUSHPTR(_heap_start);
  PUSHPTR(_utcb_start);
  ficlEvaluate(pVM, 
               "constant utcb-start "
               "constant heap-start "
               "constant hip "
               /* XXX Cause page fault at heap start */
               "heap-start q@ "
               );
  __exit(3);
}

/* EOF */
