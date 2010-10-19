/**
 * Compiler-specific annotations
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

/* EOF */
