/*
 * Nova syscall bindings, asm part.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of Vancouver.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
	.global __start
	.section .text.__start
__start:
	mov	$stack, %eax
	xchg	%eax, %esp
	sub	$0x1000, %eax
	// push utcb
	push	%eax
	add	$0x1000, %eax
	call	start
	ud2a

	.section .text.idc_reply_and_wait_fast
	.global idc_reply_and_wait_fast
idc_reply_and_wait_fast:
	// we come here with a ret from a portal function
	// w2: utcb->mtr
	mov	%eax, %esi
	// w0: NOVA_IPC_REPLY
	mov	$1, %eax

	// ecx: stack - it starts from the top
	pushl 	$idc_reply_and_wait_fast
	mov	%esp, %ecx
	sysenter

	.section .bss.stack
	.align 0x1000
	.space 0x1000
stack:
