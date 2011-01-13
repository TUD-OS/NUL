/*
 * \brief   Header of boot_linux.S
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


#ifndef _BOOT_LINUX_H_
#define _BOOT_LINUX_H_

void jmp_kernel(unsigned cs, unsigned stack) __attribute__((noreturn));

#endif
