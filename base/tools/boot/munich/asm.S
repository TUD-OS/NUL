/*
 * \brief   ASM functions like do_skinit or reboot.
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


.macro	FUNCTION name
	.section .text.\name
	.globl \name
	\name:
.endm

FUNCTION __mbheader
	.align  4, 0x90
	.long   0x1BADB002              /* magic */
	.long   0x00000000              /* feature flags */
	.long   0 - 0x1BADB002

FUNCTION __start
	leal    _stack,%esp
	xchg    %eax,%edx
	xchg    %ebx,%eax
	pushl	%eax
	pushl   $__exit
	jmp     __main


FUNCTION reboot
	mov	$0x4, %al
	outb	%al, $0x60
	mov	$0xFE, %al
	outb	%al, $0x64
	lidt    dummy_idt_desc
	ud2
       .bss
dummy_idt_desc:
	.space 8

/* the gdt to load after skinit */
FUNCTION gdt
	.global pgdt_desc
	.align(8)
pgdt_desc:
	.word end_gdt - gdt - 1
	.long gdt
	.word 0
_gdt_cs:
	.word 0xffff
	.word 0x0
	.word 0x9b00
	.word 0x00cf
_gdt_ds:
	.word 0xffff
	.word 0x0
	.word 0x9300
	.word 0x00cf
end_gdt:

	/* our stack */
	.globl  _stack
	.bss
_stack_end:
	.space  512
_stack:
