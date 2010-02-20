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
  ENTRY(_start)
   .  =  0x010000;
   . +=  SIZEOF_HEADERS;
  .text :
  {
    *(.text._start)
    *(.text .text.* .gnu.linkonce.t.*);
  } : text

  . = ALIGN(0x1000);
  _utcb_start = .;
  . += 0x100000;
  _utcb_end   = .;

  .data :
  {
    *(.rodata .rodata.* .gnu.linkonce.r.*);
    *(.data   .data.*   .gnu.linkonce.d.*);
    *(.bss .bss.* .gnu.linkonce.b.*);
    *(COMMON);
    . = ALIGN(0x10);
    _early_heap_start = .;
    . += 0x20000;
    _early_heap_end = .;
  } : data

  . = ALIGN(0x1000);
  _heap_start = .;

  /DISCARD/ :
  {
    *(.comment);
  }
}
