extern loop
[bits 64]

section .data
text: db "idk", 3, 0

section .text
loop:
    mov rax, 8
    lea rdi, [rel text]
    mov rsi, 21
    int 0x69
    jmp loop
