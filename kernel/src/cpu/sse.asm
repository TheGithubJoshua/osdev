global enable_sse
enable_sse:
    mov eax, 1
    cpuid
    test edx, 1 << 25
    jz .noSSE

    ; Initialize x87 FPU
    finit

    ; Enable SSE in CR0/CR4
    mov rax, cr0
    and rax, 0xFFFFFFFFFFFFFBFF   ; clear EM
    or  rax, 0x2                  ; set MP
    mov cr0, rax

    mov rax, cr4
    or  rax, (3 << 9)             ; OSFXSR + OSXMMEXCPT
    mov cr4, rax

.noSSE:
    ret