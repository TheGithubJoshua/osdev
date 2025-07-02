extern loop
[bits 64]

section .data
text: db "idk", 3, 0
filename: db "test.txt", 8, 0 

section .text
loop:
    mov rax, 8
    lea rdi, [rel text]
    mov rsi, 21
    int 0x69
    ; try mapping some memory
    mov rax, 4
    mov rdi, 0x00800000
    mov rsi, 0x00800000
    mov rdx, 0x5 ; present, user
    int 0x69
    mov rax, qword [0x00800000] ; attempt access
    mov rax, 0 ; read file 
    lea rdi, [rel filename]
    int 0x69
    mov rax, 8 ; print file contents
    ; rdi set by file read syscall
    mov rsi, 50 ; idfk length
    int 0x69
    ;jmp loop
    ; exit instead
    mov rax, 9
    int 0x69
