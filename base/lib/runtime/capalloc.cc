/**
 * @file
 * 
 * Copyright (C) 2011, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <nul/program.h>

// cap map - maintains the list of free capability regions
RegionList<512> _cap_region;
// Not multi-threaded safe - lock by your own or use separate cap allocator -> alloc_cap/dealloc_cap in nul/capalloc.h !
unsigned alloc_cap_region(unsigned count, unsigned align_order) { return _cap_region.alloc(count, align_order); }
void dealloc_cap_region(unsigned base, unsigned count) { return _cap_region.add(Region(base, count)); }
