/**
 * Standard include file.
 *
 * Copyright (C) 2007-2010, Bernhard Kauer <bk@vmmon.org>
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

extern "C" void  __exit(unsigned long status) __attribute__((noreturn));
void *malloc(unsigned long size);
void free(void *ptr);
void *memalign(unsigned long boundary, unsigned long size);
