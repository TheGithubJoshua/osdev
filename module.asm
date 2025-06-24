section .data
msg:    db 'Hello from ELF!', 0

section .text
global _start
extern serial_puts
_start:
lea rdi, [rel msg]        ; argument: pointer to msg
call serial_puts
