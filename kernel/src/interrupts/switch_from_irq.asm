global switch_from_irq
extern current_task

section .text
switch_from_irq:
    cli
    mov rdx, [rel current_task]
    mov [rdx], rsp           ; Save current stack pointer

    mov rdi, [rdx + 8]
    mov [rel current_task], rdi

    mov rsp, [rdi]           ; Load next task stack pointer

    ; Pop in reverse order of push
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; r't add rsp unless you know error code is pushed
    ret
