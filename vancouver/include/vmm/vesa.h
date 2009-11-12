/**
 * Common VESA defintions.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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
  struct InfoBlock
  {
    unsigned char  signature[4];
    unsigned short version;
    unsigned short oem_string[2];
    unsigned char  caps[4];
    unsigned short video_mode[2];
    unsigned short memory;
    unsigned short oem_revision;
    unsigned short oem_vendor[2];
    unsigned short oem_product[2];
    unsigned short oem_product_rev[2];
    unsigned char  res[222+256];
  };

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
    unsigned char  res[12];
    unsigned int   physbase;
    unsigned short res1[3];
    unsigned short bytes_per_scanline;
  };
};
