
        .global _start, _ec_template, _ec_template_end
        .extern execPtFn
        .section .text._start
_start: mov %esp, %eax
        mov $main_stack, %esp
        jmp start

_ec_template:
        .byte 0xB8 // mov imm to eax
        .long 0    // pVM
        .byte 0xBA // mov imm to edx
        .long 0    // pWord
        mov $execPtFn, %ecx
        push $_ec_reply
        jmp *%ecx  // XXX Use relative immediate call and calculate offset
_ec_template_end:

_ec_reply:
        // EAX = MTR
        // we come here with a ret from a portal function
	// w1: cond
	xor	%edi, %edi
	// w2: utcb->mtr
        mov     %eax, %esi
	// w0: NOVA_IPC_REPLY
	xor	%eax, %eax
	inc	%eax
	// ecx: stack - it starts from the top
	mov	%esp, %ecx
	// edx: return pointer - in the error case we return on the __exit function
	mov	$__exit, %edx
	sysenter

        
        .section .bss.main_stack
        .align 0x1000
        .space 0x1000
main_stack:

        // EOF
        