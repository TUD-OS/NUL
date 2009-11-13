/**
 * ELF32 structs.
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

struct eh32
{
  unsigned char  e_ident[16];
  unsigned short e_type;
  unsigned short e_machine;
  unsigned int   e_version;
  unsigned int   e_entry;
  unsigned int   e_phoff;
  unsigned int   e_shoff;
  unsigned int   e_flags;
  unsigned short e_ehsz;
  unsigned short e_phentsize;
  unsigned short e_phnum;
  unsigned short e_shentsize;
  unsigned short e_shnum;
  unsigned short e_shstrndx;
};

struct ph32 {
  unsigned int p_type;
  unsigned int p_offset;
  unsigned int p_vaddr;
  unsigned int p_paddr;
  unsigned int p_filesz;
  unsigned int p_memsz;
  unsigned int p_flags;
  unsigned int p_align;
};
