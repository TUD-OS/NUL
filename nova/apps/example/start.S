	/* -*- Mode: Asm -*- */

	.intel_syntax noprefix
	
	.global _start
	.section .text._start

	.align 4
_mbheader:
	.long 0x1BADB002	/* magic */
	.long 3			/* features */
	.long -(3 + 0x1BADB002)	/* checksum */

_start:
	mov     eax, esp	/* HIP */
        mov     esp, offset _stack
	push 	0xDEADBEEF
	jmp	main		/* main accepts regparm arguments */

	.section .mainstack
	.space 0x1000
_stack:

	/* EOF */
