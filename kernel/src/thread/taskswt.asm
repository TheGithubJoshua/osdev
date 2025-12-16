global switch_to_task
extern current_task    

section .text

; void switch_to_task(task_t *next)
; RDI = pointer to next task
switch_to_task:
    ; Save current task's RSP and FX state
    mov     rax, [current_task]
    mov     [rax], rsp
    lea     rdx, [rax + 32]
    fxsave  [rdx]

    ; Switch current_task pointer
    mov     [current_task], rdi

    ; Load new task's RSP and FX state
    mov     rsp, [rdi]
    lea     rdx, [rdi + 32]
    fxrstor [rdx]

    ret
