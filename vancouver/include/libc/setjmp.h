// -*- Mode: C++ -*-

#pragma once

#include <cstdint>

struct _jmp_buf {
  uintptr_t data[6];
};

typedef struct _jmp_buf jmp_buf[1];

extern "C" int setjmp(jmp_buf buf) __attribute__((regparm(3)));
extern "C" void longjmp(jmp_buf buf, int val) __attribute__((regparm(3), noreturn));

// EOF
