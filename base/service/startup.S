        ## -*- Mode: Asm -*-

        ## The startup function and the initial stack.  Called with
        ## the HIP in %esp and the UTCB a page below the HIP.

        .global __start
        .section .text.__start

__start:
        ## EAX contains our CPU number (first parameter to start)
        mov     %esp,   global_hip
        lea     -0x1000(%esp), %edx
        mov     $stack, %esp
        push    %edx            # push UTCB -- needed for myutcb()
        sub     $12, %esp       # alignment
        call	start
        ud2a

        .global global_hip
        .section .bss.hip
global_hip: .space 4


        .section .bss.stack
        .align 0x1000
        .space 0x1000
stack:

        ## A fast reply to our client, called by a return to a portal
        ## function.
        .global idc_reply_and_wait_fast
        .section .text.idc_reply_and_wait_fast
idc_reply_and_wait_fast:
        ## w0: NOVA_IPC_REPLY
	mov     $1,     %al
	## keep a pointer to ourself on the stack
	## ecx: stack
	lea     -4(%esp), %ecx
	sysenter

        ## EOF
        