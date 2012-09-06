/** @file
 * Common VESA defintions.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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

struct Vbe
{
  enum {
    TAG_VBE2 = 0x32454256,
    TAG_VESA = 0x41534556,
    MAX_VESA_MODES = 222/2
  };

  struct InfoBlock
  {
    unsigned int   tag;
    unsigned short version;
    unsigned int   oem_string;
    unsigned int   caps;
    unsigned int   video_mode_ptr;
    unsigned short memory;
    unsigned short oem_revision;
    unsigned int   oem_vendor;
    unsigned int   oem_product;
    unsigned int   oem_product_rev;
    char  scratch[222+256];
  } __attribute__((packed));

  struct ModeInfoBlock
  {
    unsigned short attr;
    unsigned short win[7];
    unsigned short bytes_scanline;
    unsigned short resolution[2];
    unsigned char  char_size[2];
    unsigned char  planes;
    unsigned char  bpp;
    unsigned char  banks;
    unsigned char  memory_model;
    unsigned char  bank_size;
    unsigned char  number_images;
    unsigned char  res0;
    unsigned char  vbe1[8];
    unsigned char  colormode;
    // vbe2
    unsigned int   phys_base;
    unsigned short res1[3];
    // vbe3
    unsigned short bytes_per_scanline;
    unsigned char  number_images_bnk;
    unsigned char  number_images_lin;
    unsigned char  vbe3[12];
    // own extensions
    unsigned char  res2;
    unsigned int   _phys_size; // framebuffer size
    unsigned short _vesa_mode; // vesa mode number
    //unsigned char  res2[189 - 7];
  } __attribute__((packed));
};
