/*
 * \brief   ASM functions to start a linux kernel.
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


.macro	FUNCTION name
	.section .text.\name
	.globl \name
	\name:
.endm



/**
 * Setup stack, disable protection mode and jump to the linux bootstrap code via lret.
 */
FUNCTION jmp_kernel
	// assemble stack frame
	xor    %ecx, %ecx
	mov    %edx, %esp
	push   %eax
	push   %ecx

	// fix stack pointer
	mov    %esp, %edx
	movzx  %dx, %esp
	xor    %dx, %dx
	shr    $0x4, %edx

	// load cs and ss with 16bit operand size
	lgdt    real_pgdt_desc
	mov	$0x10, %eax
	mov	%ax, %ss
	ljmp    $0x8, $1f
	1:

	// jmp to linux
	.code16
	movl    %ecx, %cr0
	mov	%dx,   %ss
	mov	%dx,   %ds
	mov	%dx,   %es
	mov	%dx,   %fs
	mov	%dx,   %gs
	lretl
	.code32


/* the gdt to load before jmp to real-mode*/
FUNCTION real_gdt
	.align(8)
real_pgdt_desc:
	.word real_end_gdt - real_gdt - 1
	.long real_gdt
	.word 0
_gdt_real_cs:
	.word 0xffff
	.word 0x0
	.word 0x9f00
	.word 0x008f
_gdt_real_ds:
	.word 0xffff
	.word 0x0
	.word 0x9300
	.word 0x0000
real_end_gdt:
