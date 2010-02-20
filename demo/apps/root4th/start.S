
        .global _start
        .section .text._start
_start: mov %esp, %eax
        mov $main_stack, %esp
        jmp start

        .section .bss.main_stack
        .align 0x1000
        .space 0x1000
main_stack:

        // EOF
        