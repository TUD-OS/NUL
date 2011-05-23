/*
 * \brief   multiboot structures
 * \date    2006-03-28
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006,2007,2010  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */


#pragma once

enum mbi_enum
  {
    MBI_MAGIC                  = 0x2badb002,
    MBI_FLAG_MEM               = 1 << 0,
    MBI_FLAG_CMDLINE           = 1 << 2,
    MBI_FLAG_MODS              = 1 << 3,
    MBI_FLAG_MMAP              = 1 << 6,
    MBI_FLAG_BOOT_LOADER_NAME  = 1 << 9,
    MBI_FLAG_VBE               = 1 << 11,
  };


struct mbi
{
  unsigned flags;
  unsigned mem_lower;
  unsigned mem_upper;
  unsigned boot_device;
  unsigned cmdline;
  unsigned mods_count;
  unsigned mods_addr;
  unsigned dummy0[4];
  unsigned mmap_length;
  unsigned mmap_addr;
  unsigned dummy1[3];
  unsigned boot_loader_name;
};


struct module
{
  unsigned mod_start;
  unsigned mod_end;
  unsigned string;
  unsigned reserved;
};


struct mmap
{
  unsigned size;
  unsigned long long base __attribute__((packed));
  unsigned long long length  __attribute__((packed));
  unsigned type;
};
