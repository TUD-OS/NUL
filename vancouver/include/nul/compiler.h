/**
 * Compiler-specific annotations
 *
 * Copyright (C) 2010-2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#if !defined(__GNUC__)
#error Your platform is not supported.
#endif

#define REGPARM(x) __attribute__((regparm(x)))
#define NORETURN   __attribute__((noreturn))
#define PURE       __attribute__((pure))
#define COLD       __attribute__((cold))
#define ALIGNED(x) __attribute__((aligned(x)))
#define MEMORY_BARRIER __asm__ __volatile__ ("" ::: "memory")

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
# define WARN_UNUSED __attribute__((warn_unused_result))
#else
# define WARN_UNUSED
#endif

/* EOF */
