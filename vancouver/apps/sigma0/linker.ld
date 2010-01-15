/*
 * Sigma0 linker script.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of Vancouver.
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

SECTIONS
{
  ENTRY(__start)
   .  =  0x00400000;
   __image_start = .;
   . +=  SIZEOF_HEADERS;
  .text :
  {
    *(.text .text.* .gnu.linkonce.t.*);
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
    *(.rodata .rodata.* .gnu.linkonce.r.*);
    *(.data   .data.*   .gnu.linkonce.d.*);
    *(.bss .bss.* .gnu.linkonce.b.*);
    __mempoolstart = .;
    . += 0x80000;
    __mempoolend = .;
  } : data
   __image_end = .;

  /DISCARD/ :
  {
    *(.comment);
  }
}
