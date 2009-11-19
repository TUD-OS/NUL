	.section .text.reply_and_wait_fast
	.global reply_and_wait_fast
reply_and_wait_fast:
	// We come here via a return from a portal function, which was
	// called from an empty stack. The first word on the stack is

        mov     (%esp),  %esi	// ESI = &utcb
        mov     4(%esp), %esi	// ESI = utcb->mtr

        xor     %eax, %eax
        inc     %eax            // EAX = REPLY

	// ECX contains the new stack address.
	pushl	$reply_and_wait_fast
	mov	%esp, %ecx

	// EDX return address. Only used in case of an error.
	mov	$_abort, %edx
	sysenter

	// EOF
