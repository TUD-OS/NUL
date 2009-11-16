	.intel_syntax noprefix

	.global _start
	.section .text._start
_start:
	// ESP points to HIP. Below the HIP is our initial UTCB, which
	// is always 4K at the moment.
	mov	eax, esp

	// Setup initial stack and let the first word on the stack
	// point to our UTCB.
	mov	esp, offset stack
	push 	eax
	sub	[dword ptr esp], 0x1000

	push	offset _abort
	jmp	start

	.section .text._abort
	.weak _abort
	.global _abort
_abort:
	ud2a

	.section .bss.stack
	.align 0x4000
	.space 0x4000
stack:

	// EOF
