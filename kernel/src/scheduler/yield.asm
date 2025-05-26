global yield
extern current_task
extern switch_to_task
extern task_free

section .text

; void yield(void)
yield:
    ; Save callee-saved registers (minimal: rbx, rbp, r12–r15)
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rbp

    ; free resources if task is dead
    call task_free
    
    ; Save current RSP into current_task->rsp
    mov rax, [current_task]
    mov [rax], rsp

    ; Get the next task in the list (current_task->next)
    mov rdi, [rax + 8]      ; rdi = current_task->next


    ; Call switch_to_task(next)
    call switch_to_task

    ; Restore callee-saved registers (from new task’s stack)
    pop rbp
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15

    ret