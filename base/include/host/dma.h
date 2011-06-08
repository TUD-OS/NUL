/** @file
 * Generic DMA definitions.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
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

#include "service/string.h"
#include "service/helper.h"

struct DmaDescriptor
{
  unsigned long byteoffset;
  unsigned bytecount;


  static unsigned long sum_length(unsigned char dmacount, DmaDescriptor *dma)
  {
    unsigned long res = 0;
    for (unsigned i=0; i < dmacount; i++) res += dma[i].bytecount;
    return res;
  }

  /**
   * Copy data from an internal buffer to an DMA buffer.
   */
  static bool copy_inout(char *buffer, unsigned len, unsigned long offset,
			 unsigned char dmacount, DmaDescriptor *dma, bool copyout,
			 unsigned long physoffset, unsigned long physsize)
  {
    unsigned i;
    for (i=0; i < dmacount && offset >= dma[i].bytecount; i++)  offset -= dma[i].bytecount;
    while (len > 0 && i < dmacount)
      {
	assert(dma[i].bytecount >= offset);
	unsigned sublen = dma[i].bytecount - offset;
	if (sublen > len) sublen = len;

	if ((dma[i].byteoffset + offset) > physsize ||  (dma[i].byteoffset + offset + sublen) > physsize) break;

	if (copyout)
	  memcpy(reinterpret_cast<char *>(dma[i].byteoffset + physoffset) + offset, buffer, sublen);
	else
	  memcpy(buffer, reinterpret_cast<char *>(dma[i].byteoffset + physoffset) + offset, sublen);

	buffer += sublen;
	len -= sublen;
	i++;
	offset = 0;
      }
    return len > 0;
  }

};



/**
 * The parameters to distinguish different drives.
 */
struct DiskParameter
{
  enum {
    FLAG_HARDDISK = 1,
    FLAG_ATAPI    = 2
  };
  unsigned flags;
  unsigned long long sectors;
  unsigned sectorsize;
  unsigned maxrequestcount;
  char name[256];
};
