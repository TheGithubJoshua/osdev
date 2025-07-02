global switch_to_task
extern current_task    

section .text

; void switch_to_task(task_t *next)
; RDI = pointer to next task
switch_to_task:
    ; Load the pointer stored in current_task into RAX
    mov     rax, [current_task]   ; current_task points to the current task
    mov     [rax], rsp            ; Save current RSP into current_task->rsp

    ; Set current_task = next
    mov     [current_task], rdi

    ; Load CR3 from next->cr3
    ;mov     rax, [rdi + 16]            ; cr3 offset (rsp is at +0, cr3 at +8)
    ;mov     cr3, rax                  ; switch address space

    ; Load new RSP from next->rsp
    mov     rsp, [rdi]

    ; Return into new task
    ret
