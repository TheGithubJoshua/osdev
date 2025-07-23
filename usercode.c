static inline long syscall(long syscall_num, long arg1, long arg2, long arg3) {
    long result;
    __asm__ volatile (
        "mov %1, %%rax\n\t"
        "mov %2, %%rdi\n\t"
        "mov %3, %%rsi\n\t"
        "mov %4, %%rdx\n\t"
        "int $0x69\n\t"
        "mov %%rax, %0\n\t"
        : "=r" (result)
        : "r" (syscall_num), "r" (arg1), "r" (arg2), "r" (arg3)
        : "rax", "rdi", "rsi", "rdx", "memory"
    );
    return result;
}

// Data section equivalents
static char text[] = {'i', 'd', 'k', 3, 0};
static char filename[] = {'h', 'e', 'l', 'l', 'o', '.', 't', 'x', 't', 9, 0};
static char path[] = {'t', 'e', 's', 't', '.', 't', 'x', 't', 0};

// BSS section equivalent
static char buf[69];

// External function declaration (entry point)
extern void _start(void);
static const char fn[100] = "test.txt";
void _start(void) {
    // Infinite loop equivalent
    for (;;) {
        // Commented out sections from original assembly
        /*
        // Print text
        syscall(8, (long)text, 21, 0);
        
        // Map memory
        syscall(4, 0x00800000, 0x00800000, 0x5);
        
        // Read file
        syscall(0, (long)filename, 0, 0);
        
        // Print file contents  
        syscall(8, 0, 50, 0); // rdi set by file read syscall
        */
        
        // Open file
        long fd = syscall(2, (const char)fn, 0, 0);
        
        // Read from file
        syscall(0, fd, (long)buf, 69);
        
        // Print buffer contents
        syscall(8, (long)buf, 69, 0);
        
        // Exit cleanly
        syscall(9, 0, 0, 0);
    }
}
