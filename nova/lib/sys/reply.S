	.intel_syntax noprefix

	.section .text._reply_and_wait
	.global _reply_and_wait
_reply_and_wait_fast:
	// We come here via a return from a portal function, which was
	// called from an empty stack. The first word on the stack is

	mov	esi, [esp]	// ESI = &utcb
	mov	esi, [esp + 4]	// ESI = utcb->mtr

	xor	eax, eax
	inc	eax		// EAX = REPLY

	// ECX contains the new stack address.
	push	offset _reply_and_wait_fast
	mov	ecx, esp

	// EDX return address. Only used in case of an error.
	mov	edx, offset _abort
	sysenter

	// EOF
