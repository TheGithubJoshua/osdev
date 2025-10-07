#ifndef LB_SIZE
#define LB_SIZE 128
#endif

typedef unsigned long uint64_t;
typedef unsigned long size_t;
typedef unsigned char uint8_t;

// Syscall function for print
static inline void sys_print(const void *buf, size_t len) {
    register uint64_t rax asm("rax") = 8;      // syscall number
    register const void *rdi asm("rdi") = buf;
    register size_t rsi asm("rsi") = len;
    asm volatile(
        "int $0x69"
        :
        : "r"(rax), "r"(rdi), "r"(rsi)
        : "memory"
    );
}

static inline void sys_serial_puts(const char buf[]) {
    asm volatile(
        "int $0x69"
        :
        : "a"((uint64_t)5), "D"(buf)
        : "memory"
    );
}

static inline void sys_serial_putc(char buf) {
    asm volatile(
        "int $0x69"
        :
        : "a"((uint64_t)7), "D"(buf)
        : "memory"
    );
}

static inline void sys_exit() {
    register uint64_t rax asm("rax") = 9;      // syscall number
    asm volatile(
        "int $0x69"
        :
        : "r"(rax)
        : "memory"
    );
}

static inline uint64_t sys_terminal_read(void) {
    uint64_t ret;
    asm volatile(
        "int $0x69\n\t"
        : "=a"(ret)                   // output: rax
        : "a"((uint64_t)11)           // input: rax (syscall number)
        : "memory"
    );
    return ret;
}

void main() {
    sys_serial_puts("incoming buffer first char:");
    sys_serial_putc((char)sys_terminal_read());
    sys_exit();
}
