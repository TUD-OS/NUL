/*
 * Linker script for Vancouver.nova.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
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
   .  =  0x80000000;
   __image_start = .;
   . +=  SIZEOF_HEADERS;

  .text :
  {
     /* NUL magic */
     LONG(0x204c554e);
     /* requested API version */
     LONG(3);
    *(.text .text.* .gnu.linkonce.t.*);
  } : text
  . = ALIGN(0x1000);


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
  } : data
  .bss :
  {
    *(.bss .bss.* .gnu.linkonce.b.*);
    *(COMMON);
     __mempoolstart = .;
    . += __memsize;
    . = ALIGN(0x1000);
    __mempoolend = .;
  }
  __image_end = .;
  /* the freememory starts here, we align to 4M to allow superpages */
  __freemem = ALIGN(0x400000);


  /DISCARD/ :
  {
    *(.comment);
  }
}
