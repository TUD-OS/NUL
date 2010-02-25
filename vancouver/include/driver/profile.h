/**
 * Profiling support.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <bk@vmmon.org>
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

#ifdef __LP64__
#define PVAR  ".quad"
#else
#define PVAR  ".long"
#endif

#define COUNTER_INC(NAME)						\
  ({									\
    long __res;								\
    asm volatile (".section .data; 1: .string \"" NAME "\";.previous;"	\
		  ".section .profile; " PVAR " 1b; 2: " PVAR " 0,0;.previous;" \
		  "incl 2b; mov 2b, %0" : "=q"(__res));			\
    __res;								\
  })


#define COUNTER_SET(NAME, VALUE)					\
  {									\
    asm volatile (".section .data; 1: .string \"" NAME "\";.previous;"	\
		  ".section .profile; "  PVAR " 1b; 2: " PVAR " 0,0;.previous;" \
		  "mov %0,2b" : : "q"(VALUE));				\
  }
