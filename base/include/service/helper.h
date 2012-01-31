/** @file
 * Helper functions.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
#pragma once

void * operator new[](unsigned size, unsigned alignment);
void * operator new(unsigned size, unsigned alignment);
void  do_exit(const char *msg) __attribute__((noreturn));
extern void *(*memalloc)(unsigned long size, unsigned long align);
extern void (*memfree)(void *ptr);

/* Simple malloc. Used as backend for other allocators. */
extern void *memalloc_mempool(unsigned long size, unsigned long align);
extern void memfree_mempool(void *ptr);


// Not multi-threaded safe - lock by your own or use separate cap allocator -> alloc_cap/dealloc_cap in nul/capalloc.h !
unsigned alloc_cap_region(unsigned count, unsigned align);
void dealloc_cap_region(unsigned base, unsigned count);


/**
 * Assert.
 */
#ifdef NDEBUG
#define assert(X) do {} while (0)
#else
#define do_string2(x) do_string(x)
#define do_string(x) #x
#define assert(X) do { if (!(X)) do_exit("assertion '" #X  "' failed in "  __FILE__  ":" do_string2(__LINE__) ); } while (0)
#endif

/**
 * Check whether some address is in a range.
 */
static inline bool in_range(unsigned long address, unsigned long base, unsigned long size) { return (base <= address) && (address <= base + size - 1); }

/**
 * Check whether X is true, output an error and return.
 */
#define check0(X, ...) ({ unsigned __res; if ((__res = (X))) { Logging::printf("%s() line %d: '" #X "' error = %x", __func__, __LINE__, __res); Logging::printf(" " __VA_ARGS__); Logging::printf("\n"); return; }})

/**
 * Check whether X is true, output an error and return RET.
 */
#define check1(RET, X, ...) ({ unsigned __res; if ((__res = (X))) { Logging::printf("%s() line %d: '" #X "' error = %x", __func__, __LINE__, __res); Logging::printf(" " __VA_ARGS__); Logging::printf("\n"); return RET; }})

/**
 * Check whether X is true, output an error and goto RET.
 */
#define check2(GOTO, X, ...) ({ if ((res = (X))) { Logging::printf("%s() line %d: '" #X "' error = %x", __func__, __LINE__, res); Logging::printf(" " __VA_ARGS__); Logging::printf("\n"); goto GOTO; }})

/**
 * Make a dependency on the argument, to avoid that the compiler will touch them.
 */
#define asmlinkage_protect(...) __asm__ __volatile__ ("" : : __VA_ARGS__);
