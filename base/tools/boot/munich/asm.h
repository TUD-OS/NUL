/*
 * \brief   ASM inline helper routines.
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

void reboot(void) __attribute__((noreturn));

static inline
unsigned char
inb(const unsigned short port)
{
  unsigned char res;
  asm volatile("inb %1, %0" : "=a"(res): "Nd"(port));
  return res;
}

static inline
void
outb(const unsigned short port, unsigned char value)
{
  asm volatile("outb %0,%1" :: "a"(value),"Nd"(port));
}

static inline
unsigned
bsr(unsigned int value)
{
  unsigned res;
  asm volatile("bsr %1,%0" : "=r"(res): "r"(value));
  return res;
}
