global jump_usermode
extern test_user_function
extern stack_top

jump_usermode:
    cli
    ; Load stack top (user RSP)
    mov rax, [stack_top]

    ; Push SS
    push qword 0x23              ; user data segment (RPL=3)
    ;sub rax, 8
    ;and rax, -16
    push rax                     ; user stack pointer (RSP)

    ; Push RFLAGS
    push qword 0x202             ; IF=1, default flags

    ; Push CS
    push qword 0x1B              ; user code segment (RPL=3)

    ; Push RIP (entry point)
    mov rax, 0x400000
    push rax

    ; Done: perform the transition
    iretq