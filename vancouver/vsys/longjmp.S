        // -*- Mode: Asm -*-

        .globl longjmp, setjmp
        .section .text.longjmp
setjmp:
        // eax - pointer to jmp_buf
        mov %esp, (%eax)
        mov %ebx, 4(%eax)
        mov %edi, 8(%eax)
        mov %esi, 12(%eax)
        mov %ebp, 16(%eax)
        mov (%esp), %ecx
        mov %ecx, 20(%eax)

        xor %eax, %eax
        ret

longjmp:
        // eax - pointer to jmp_buf
        // edx - return value
        mov (%eax), %esp
        mov 4(%eax), %ebx
        mov 8(%eax), %edi
        mov 12(%eax), %esi
        mov 16(%eax), %ebp
        mov 20(%eax), %ecx
        pop %eax                // Discard old return address
        mov %edx, %eax
        jmp *%ecx

        // EOF
