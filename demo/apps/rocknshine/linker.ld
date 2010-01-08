/*
 * Linker script for Vancouver.nova.
 *
 * Copyright (C) 2008, Bernhard Kauer <kauer@tudos.org>
 *
 * This file is part of Vancouver.nova.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

EXTERN(__start)

PHDRS
{
	text     PT_LOAD FLAGS(5) FILEHDR PHDRS;
	data     PT_LOAD FLAGS(6);
}

SECTIONS
{
  ENTRY(_start)
   .  =  0x80000000;
   __image_start = .;
   . +=  SIZEOF_HEADERS;
  .text :
  {
    *(.text .text.*);
    . = ALIGN(0x1000);
  } : text
 
  .data :
  {
    __profile_table_start = .;
    KEEP(*(.profile));
    __profile_table_end = .;
    __param_table_start = .;
    KEEP(*(.param));
    __param_table_end = .;
    *(.rodata .rodata.*);
    *(.data);
    *(.data.*);
   __image_end = .;
  } : data
  .bss :
  {
    *(COMMON);
    *(.bss);
    *(.bss.*);
    __mempoolstart = .;
    . += 0x800000;
    __mempoolend = .;
    __module_start = .;
    . += 32M;
    __module_end = .;
  }

  /DISCARD/ :
  {
    *(.comment);
  }
}
