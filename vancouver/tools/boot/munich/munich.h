/*
 * \brief   header of munich.c
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

#include "mbi.h"

enum linux_enum
  {
    LINUX_HEADER_MAGIC        = 0x53726448,
    LINUX_BOOT_FLAG_MAGIC     = 0xAA55,
  };

struct linux_kernel_header
{
  unsigned char     setup_sects;
  unsigned short    root_flags;
  unsigned int      syssize;
  unsigned short    ram_size;
  unsigned short    vid_mode;
  unsigned short    root_dev;
  unsigned short    boot_flag;
  unsigned short    jump;
  unsigned int      header;
  unsigned short    version;
  unsigned int      realmode_swtch;
  unsigned short    start_sys;
  unsigned short    kernel_version;
  unsigned char     type_of_loader;
  unsigned char     loadflags;
  unsigned short    setup_move_size;
  unsigned int      code32_start;
  unsigned int      ramdisk_image;
  unsigned int      ramdisk_size;
  unsigned int      bootsect_kludge;
  unsigned short    heap_end_ptr;
  unsigned short    pad1;
  unsigned int      cmd_line_ptr;
  unsigned int      initrd_addr_max;
} __attribute__((packed));


int _main(struct mbi *local_mbi, unsigned flags);
