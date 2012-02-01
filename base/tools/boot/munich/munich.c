/*
 * \brief   Munich - starts Linux
 * \date    2006-06-28
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

#include "version.h"
#include "util.h"
#include "munich.h"
#include "boot_linux.h"

const char *message_label = "MUNICH: ";

const unsigned REALMODE_STACK = 0x49000;
const unsigned REALMODE_IMAGE = 0x40000;

/**
 * Starts a linux from multiboot modules. Treats the first module as
 * linux kernel and the optional second module as initrd.
 */
__attribute__((noreturn)) int
start_linux(struct mbi *mbi)
{
  struct module *m  = (struct module *) (mbi->mods_addr);
  struct linux_kernel_header *hdr = (struct linux_kernel_header *)(m->mod_start + 0x1f1);

  // sanity checks
  ERROR(-11, ~mbi->flags & MBI_FLAG_MODS, "module flag missing");
  ERROR(-12, !mbi->mods_count, "no kernel to start");
  ERROR(-13, 2 < mbi->mods_count, "do not know what to do with that many modules");
  ERROR(-14, LINUX_BOOT_FLAG_MAGIC != hdr->boot_flag, "boot flag does not match");
  ERROR(-15, LINUX_HEADER_MAGIC != hdr->header, "too old linux version?");
  ERROR(-16, 0x202 > hdr->version, "can not start linux pre 2.4.0");
  ERROR(-17, !(hdr->loadflags & 0x01), "not a bzImage?");

  // filling out the header
  hdr->type_of_loader = 0x7;      // fake GRUB here
  hdr->cmd_line_ptr   = REALMODE_STACK;

  // enable heap
  hdr->heap_end_ptr   = (REALMODE_STACK - 0x200) & 0xffff;
  hdr->loadflags     |= 0x80;

  // output kernel version string
  if (hdr->kernel_version)
    {
      ERROR(-18, hdr->setup_sects << 9 < hdr->kernel_version, "version pointer invalid");
      out_info((char *)(m->mod_start + hdr->kernel_version + 0x200));
    }

  char *cmdline = (char *) m->string, *cmd, *tok;

  // remove kernel image name from cmdline
  get_arg(&cmdline, ' ');

  // parse remaining cmdline into whitespace-separated tokens
  for (cmd = cmdline; (tok = get_arg(&cmd, ' ')); *tok ? *--tok = '=' : 0, *cmd ? *--cmd = ' ' : 0)
    {
      // parse tokens into "arg=val" pairs
      char *arg = get_arg(&tok, '=');
      if (!strcmp(arg, "vga"))
        {
          hdr->vid_mode = strtoul(tok, 0, 0);
          out_description("video mode", hdr->vid_mode);
        }
    }

  out_info(cmdline);

  // handle initrd
  if (1 < mbi->mods_count)
    {
      hdr->ramdisk_size = (m+1)->mod_end - (m+1)->mod_start;
      hdr->ramdisk_image = (m+1)->mod_start;
      if (hdr->ramdisk_image + hdr->ramdisk_size > hdr->initrd_addr_max)
	{
	  unsigned long dst = (hdr->initrd_addr_max - hdr->ramdisk_size + 0xfff) & ~0xfff;
	  out_description("relocating initrd", dst);
	  memcpy((char *)dst, (char *)hdr->ramdisk_image, hdr->ramdisk_size);
	  hdr->ramdisk_image = dst;
	}
      out_description("initrd",  hdr->ramdisk_image);
    }

  out_info("copy image");
  memcpy((char *) REALMODE_IMAGE, (char *) m->mod_start, (hdr->setup_sects+1) << 9);
  memcpy((char *) hdr->cmd_line_ptr, cmdline, strlen(cmdline)+1);
  memcpy((char *) hdr->code32_start, (char *)  m->mod_start + ((hdr->setup_sects+1) << 9), hdr->syssize*16);

  out_info("start kernel");
  jmp_kernel(REALMODE_IMAGE / 16 + 0x20, REALMODE_STACK);
}


/**
 * Start a linux from a multiboot structure.
 */
__attribute__((noreturn)) void
__main(struct mbi *mbi, unsigned flags)
{
#ifndef NDEBUG
  serial_init();
#endif
  out_info(VERSION " starts Linux");
  ERROR(10, !mbi || flags != MBI_MAGIC, "Not loaded via multiboot");
  ERROR(11, start_linux(mbi), "start linux failed");
}
