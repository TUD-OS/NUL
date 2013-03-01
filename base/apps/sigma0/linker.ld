/*
 * Sigma0 linker script.
 *
 * Copyright (C) 2008-2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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
	text     PT_LOAD FLAGS(5);
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
       /* NOVA header */
       LONG(0x41564f4e);
       __nova_api_version = .;
       LONG(6);
       *(.text .text.* .gnu.linkonce.t.*);
       . = ALIGN(0x1000);
  } : text

  .data :
  {
    __mempoolstart = .;
    . += __memsize;
    __mempoolend = .;
    __profile_table_start = .;
    KEEP(*(.profile));
    __profile_table_end = .;
    __param_table_start = .;
    KEEP(*(.param));
    __param_table_end = .;
    *(.rodata .rodata.* .gnu.linkonce.r.*);
    *(.data   .data.*   .gnu.linkonce.d.*);
    *(.bss .bss.* .gnu.linkonce.b.*);
    *(COMMON);
  } : data
   __image_end = .;

  /DISCARD/ :
  {
    *(.comment);
  }
}
