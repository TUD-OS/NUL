/**
 * ELF decoding.
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
#include "elf32.h"

#define check(X, ...) ({ unsigned __res; if ((__res = X)) { Logging::printf("%s() line %d: '" #X "' error = %x", __func__, __LINE__, __res); Logging::printf(" " __VA_ARGS__); Logging::printf("\n"); return 0xbead; }})


class Elf
{
 public:
  /**
   * Decode an elf32 binary.
   */
  static unsigned  decode_elf(char *module, char *phys_mem, unsigned long &rip, unsigned long &maxptr, unsigned long mem_size, unsigned long mem_offset) __attribute__((always_inline))
  {
    struct eh32 *elf = reinterpret_cast<struct eh32 *>(module);
    rip = elf->e_entry;
    check(!(*reinterpret_cast<unsigned *>(elf->e_ident) == 0x464c457f && *reinterpret_cast<short *>(elf->e_ident+4) == 0x0101));
    check(!(elf->e_type==2 && elf->e_machine==0x03 && elf->e_version==1));
    check(!(sizeof(struct ph32) <= elf->e_phentsize));
    for (unsigned j=0; j<elf->e_phnum; j++)
      {
	struct ph32 *ph = reinterpret_cast<struct ph32 *>(module + elf->e_phoff + j*elf->e_phentsize);
	if (ph->p_type != 1)  continue;
	check(!(mem_size > ph->p_paddr + ph->p_memsz - mem_offset), "elf section out of memory %lx vs %x ofs %lx", mem_size, ph->p_paddr + ph->p_memsz, mem_offset);
	memcpy(phys_mem + ph->p_paddr - mem_offset, module + ph->p_offset, ph->p_filesz);
	memset(phys_mem + ph->p_paddr - mem_offset + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
	if (maxptr < ph->p_memsz + ph->p_paddr - mem_offset)
	  maxptr = ph->p_paddr + ph->p_memsz - mem_offset;
      }
    Logging::printf("\t\tdecode elf rip %lx\n", rip);
    return 0;
  }
};
