extern current_task

%macro isr_err_stub 1
isr_stub_%+%1:
    push qword %1
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    ;push rsp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    ;mov rsi, %1
    call exception_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    ;pop rsp
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16      ; Discard error code
    iretq
%endmacro
; if writing for 64-bit, use iretq instead
%macro isr_no_err_stub 1
isr_stub_%+%1:
    push qword 0
    push qword %1
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    ;push rsp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    ;mov rdi, %1
    call exception_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    ;pop rsp
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16      ; Discard error code
    iretq
%endmacro

%macro isr_irq_stub 1
isr_stub_%+%1:
extern beep
    push qword 0
    push qword %1
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    ;push rsp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    ;mov rdi, %1
    call irq_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    ;pop rsp
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16      ; Discard error code
    iretq
    ;call beep ; beep
%endmacro

global isr_stub_105
isr_stub_105:
    push qword 0
    push qword 105
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    ;push rsp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    ;mov rdi, %1
    call syscall_handler
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    ;pop rsp
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16      ; Discard error code
    iretq

extern exception_handler
extern irq_handler
extern syscall_handler

isr_no_err_stub 0
isr_no_err_stub 1
isr_no_err_stub 2
isr_no_err_stub 3
isr_no_err_stub 4
isr_no_err_stub 5
isr_no_err_stub 6
isr_no_err_stub 7
isr_err_stub    8
isr_no_err_stub 9
isr_err_stub    10
isr_err_stub    11
isr_err_stub    12
isr_err_stub    13
isr_err_stub    14
isr_no_err_stub 15
isr_no_err_stub 16
isr_err_stub    17
isr_no_err_stub 18
isr_no_err_stub 19
isr_no_err_stub 20
isr_no_err_stub 21
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28
isr_no_err_stub 29
isr_err_stub    30
isr_no_err_stub 31

isr_irq_stub 32
isr_irq_stub 33
isr_irq_stub 34
isr_irq_stub 35
isr_irq_stub 36
isr_irq_stub 37
isr_irq_stub 38
isr_irq_stub 39
isr_irq_stub 40
isr_irq_stub 41
isr_irq_stub 42
isr_irq_stub 43
isr_irq_stub 44
isr_irq_stub 45
isr_irq_stub 46
isr_irq_stub 47

global isr_stub_table
isr_stub_table:
%assign i 0 
%rep    48 
    dq isr_stub_%+i ; use DQ instead if targeting 64-bit
%assign i i+1 
%endrep
