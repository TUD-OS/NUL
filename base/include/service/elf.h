/** @file
 * ELF decoding.
 *
 * Copyright (C) 2007-2008, Bernhard Kauer <bk@vmmon.org>
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
#include "elf32.h"

struct Elf
{
  static unsigned is_not_elf(const eh32 *elf, unsigned long modsize)
  {
    check1(1, modsize < sizeof(struct eh32));
    check1(2, modsize < elf->e_phoff + elf->e_phentsize *  elf->e_phnum);
    check1(3, !(elf->e_ident[0] == 0x7f && elf->e_ident[1] == 'E' && elf->e_ident[2] == 'L' && elf->e_ident[3] == 'F'));
    check1(4, !(elf->e_ident[4] == 0x01 && elf->e_ident[5] == 0x01));
    check1(5, !(elf->e_type==2 && elf->e_machine==0x03 && elf->e_version==1));
    check1(6, !(sizeof(struct ph32) <= elf->e_phentsize));
    return 0;
  }

 public:

  /**
   * Get the size of the PT_LOAD sections.
   */
  static unsigned long loaded_memsize(char *module, unsigned long modsize)
  {
    struct eh32 *elf = reinterpret_cast<struct eh32 *>(module);
    if (is_not_elf(elf, modsize))   return 0;


    unsigned long start = ~0ul;
    unsigned long end   = 0;
    for (unsigned j=0; j < elf->e_phnum; j++)
      {
	struct ph32 *ph = reinterpret_cast<struct ph32 *>(module + elf->e_phoff + j*elf->e_phentsize);
	if (ph->p_type != 1)  continue;
	if (start > ph->p_paddr)               start = ph->p_paddr;
	if (end   < ph->p_paddr + ph->p_memsz)   end = ph->p_paddr + ph->p_memsz;
      }
    return end - start;
  }


  /**
   * Decode an elf32 binary. I.e. copy the ELF segments from ELF image
   * pointed by module to the memory at phys_mem.
   */
  static unsigned  decode_elf(char *module, unsigned long modsize, char *phys_mem, unsigned long &rip, unsigned long &maxptr, unsigned long mem_size,
			      unsigned long mem_offset, unsigned long long magic)
  {
    unsigned res;
    struct eh32 *elf = reinterpret_cast<struct eh32 *>(module);
    check1(res, res = is_not_elf(elf, modsize));
    rip = elf->e_entry;
    for (unsigned j=0; j < elf->e_phnum; j++)
      {
	struct ph32 *ph = reinterpret_cast<struct ph32 *>(module + elf->e_phoff + j*elf->e_phentsize);
	if (ph->p_type != 1)  continue;
	check1(7, modsize < ph->p_offset + ph->p_filesz);
	if (magic) {
	  check1(8, ph->p_filesz < sizeof(unsigned long long));
	  check1(8, magic != *reinterpret_cast<unsigned long long *>(module + ph->p_offset), "wrong file: magic %llx vs %llx", magic, *reinterpret_cast<unsigned long long *>(module + ph->p_offset));
	  magic = 0;
	}
	check1(9, !(mem_size >= ph->p_paddr + ph->p_memsz - mem_offset), "elf section out of memory %lx vs %x ofs %lx", mem_size, ph->p_paddr + ph->p_memsz, mem_offset);
	memcpy(phys_mem + ph->p_paddr - mem_offset, module + ph->p_offset, ph->p_filesz);
	memset(phys_mem + ph->p_paddr - mem_offset + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
	if (maxptr < ph->p_memsz + ph->p_paddr - mem_offset)
	  maxptr = ph->p_paddr + ph->p_memsz - mem_offset;
      }
    return 0;
  }
};
