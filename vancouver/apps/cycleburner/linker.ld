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
PHDRS
{
	text     PT_LOAD FLAGS(5) FILEHDR PHDRS;
	data     PT_LOAD FLAGS(6);
}

ENTRY(_start)
EXTERN(_start)

SECTIONS
{
    . =  0x80000000 + SIZEOF_HEADERS;
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
  } : data
  .bss :
  {
    *(.bss);
    *(.bss.*);
    __mempoolstart = .;
    . += 0x10000;
    __mempoolend = .;
  }


  /DISCARD/ :
  {
    *(.comment);
  }
}
