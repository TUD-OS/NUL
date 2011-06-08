/** @file
 * Generic ATA functions.
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

#include "host/dma.h"

/**
 * Helper class that unifies functions for IDE+SATA disks
 */
class HostGenericAta
{
  static void ntos_string(char *dst, char *str, unsigned len)
  {
    for (unsigned i=0; i< len/2; i++)
      {
	char value = str[i*2];
	dst[i*2] = str[i*2+1];
	dst[i*2+1] = value;
      }
  }

 public:
  bool  _lba48;
  unsigned long long _maxsector;
  unsigned _maxcount;
  char _model[40];
  bool  _atapi;
  bool  _slave;


  /**
   * Update the parameters from an identify_drive packet.
   */
  unsigned update_params(unsigned short *id, bool slave)
  {
    _slave = slave;
    unsigned char checksum = 0;
    for (unsigned i=0; i < 512; i++)
      checksum += reinterpret_cast<unsigned char *>(id)[i];
    if ((id[255] & 0xff) == 0xa5 && checksum) return 0x11;
    if (((id[0] >> 14) & 3) == 3) return 0x12;
    _atapi = id[0] >> 15;
    _maxsector = 0;

    if (!_atapi)
      {
	_lba48 = id[86] & (1 << 10);
	if (~id[49] & (1<<9))
	  // we need to have LBA and do not support the very old CHS mode anymore!
	  _maxsector = 0;
	else
	  _maxsector = _lba48 ? *reinterpret_cast<unsigned long long *>(id+100) : *reinterpret_cast<unsigned *>(id+60);
	_maxcount = (1 << (_lba48 ? 16 : 8)) - 1;
      }
    ntos_string(_model, reinterpret_cast<char *>(id+27), 40);
    for (unsigned i=40; i>0 && _model[i-1] == ' '; i--)
      _model[i-1] = 0;
    dump_description();
    Logging::printf("\n");
    return 0;
  }


  /**
   * Get the disk parameters.
   */
  unsigned get_disk_parameter(DiskParameter *params) {
    params->flags = _atapi ? DiskParameter::FLAG_ATAPI : (_maxsector ? DiskParameter::FLAG_HARDDISK : 0);
    params->sectors = _maxsector;
    params->sectorsize = _atapi ? 2048 : 512;
    params->maxrequestcount = _maxcount;
    memcpy(params->name, _model, 40);
    return 0;
  };


  /**
   * Dump an description.
   */
  void dump_description()
  {
    Logging::printf(_atapi ? " ATAPI" : " HDD");
    Logging::printf(" %40s", _model);
    if (!_atapi)
      {
	Logging::printf("%s", !_maxsector ? "<unsupported>" : (_lba48 ? " LBA48" : " LBA"));
	Logging::printf(" sectors %llx", _maxsector);
      }
    else
      _maxsector = 0x1000;
  }

 HostGenericAta() : _lba48(false), _maxsector(0), _maxcount(0), _atapi(false) {}
};
