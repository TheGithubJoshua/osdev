[bits 64]
section .data
text: db "idk", 3, 0
filename: db "hello.txt", 9, 0 
path: db "test.txt", 0
section .bss
buf: resb 69
section .text
loop:
   ; open file
   mov rax, 2           ; open syscall
   lea rdi, [rel path]  ; path to file
   mov rsi, 0           ; flags (read-only)
   mov rdx, 0           ; mode
   int 0x69
   
   ; check if open was successful (assuming negative return = error)
   cmp rax, 0
   jl exit_error        ; jump to error handling if open failed
   
   ; save file descriptor
   mov r8, rax          ; save fd in r8
   
   ; read from file
   mov rax, 0           ; read syscall
   mov rdi, r8          ; use saved file descriptor
   lea rsi, [rel buf]   ; buffer to read into
   mov rdx, 69          ; number of bytes to read
   int 0x69
   
   ; save number of bytes read
   mov r9, rax          ; save bytes read count
   
   ; print buffer contents
   mov rax, 8           ; print syscall
   lea rdi, [rel buf]   ; buffer to print
   mov rsi, r9          ; use actual bytes read, not fixed 69
   int 0x69
   
   ; close file (assuming syscall 3 for close)
   mov rax, 3           ; close syscall (you may need to check your syscall numbers)
   mov rdi, r8          ; file descriptor to close
   int 0x69
   
   ; exit cleanly
   mov rax, 9           ; exit syscall
   int 0x69

exit_error:
   ; handle open failure
   mov rax, 9           ; exit syscall
   int 0x69
