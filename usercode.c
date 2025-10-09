/*#ifndef LB_SIZE
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
*/

#ifndef LB_SIZE
#define LB_SIZE 128
#endif

typedef unsigned long uint64_t;
typedef unsigned long size_t;
typedef unsigned char uint8_t;
//typedef int bool;
#define true 1
#define false 0

// Syscall function for print
static inline void sys_print(const void *buf, size_t len) {
    register uint64_t rax asm("rax") = 8;
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

static inline void sys_serial_puthex(uint64_t buf) {
    asm volatile(
        "int $0x69"
        :
        : "a"((uint64_t)6), "D"(buf)
        : "memory"
    );
}

static inline void sys_exit() {
    register uint64_t rax asm("rax") = 9;
    asm volatile(
        "int $0x69"
        :
        : "r"(rax)
        : "memory"
    );
}

static inline void sys_acpi_sleep(uint64_t sleep_state) {
    asm volatile(
        "int $0x69"
        :
        : "a"((uint64_t)12), "D"(sleep_state)
        : "memory"
    );
}

static inline uint64_t sys_terminal_read(void) {
    uint64_t ret;
    asm volatile(
        "int $0x69\n\t"
        : "=a"(ret)
        : "a"((uint64_t)11)
        : "memory"
    );
    return ret;
}

// Utility functions
size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

void print(const char *s) {
    sys_print(s, strlen(s));
}

int strcmp(const char *s1, const char *s2)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (*p1 == *p2) {
        if (*p1 == '\0')
            return 0;
        p1++;
        p2++;
    }

    return (*p1 < *p2) ? -1 : 1;
}

bool strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return false;
        if (s1[i] == '\0') return true;
    }
    return true;
}

void memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++) {
        d[i] = (uint8_t)c;
    }
}

// Command implementations (placeholders)
void cmd_help(const char *args) {
    print("Available built-in commands:\n");
    print("  help    - Show this help message\n");
    print("  echo    - Echo text to output\n");
    print("  clear   - Clear the screen\n");
    print("  ls      - List files\n");
    print("  cat     - Display file contents\n");
    print("  halt      - Shutdown the system\n");
    print("  exit    - Exit the shell\n");
}

void cmd_echo(const char *args) {
    print(args);
    print("\n");
}

void cmd_clear(const char *args) {
    print("\033[2J\033[H");  // ANSI escape codes
}

void cmd_ls(const char *args) {
    print("not implemented");
}

void cmd_cat(const char *args) {
    print("not implemented\n");
}

void cmd_exit(const char *args) {
    print("Goodbye!\n");
    sys_exit();
}

void cmd_halt(const char *args) {
    print("System is going down!\n");
    sys_acpi_sleep(5);
}

// Parse and execute command
void execute_command(char *cmdline) {
    // Skip leading whitespace
    while (*cmdline == ' ' || *cmdline == '\t') cmdline++;
    
    if (*cmdline == '\0') return;  // Empty command
    
    // Find command name end
    char *cmd_end = cmdline;
    while (*cmd_end && *cmd_end != ' ' && *cmd_end != '\t') cmd_end++;
    
    // Extract arguments
    char *args = cmd_end;
    if (*args) {
        *args = '\0';  // Null terminate command
        args++;
        while (*args == ' ' || *args == '\t') args++;  // Skip whitespace
    }

    // Search for command
    if (!strcmp(cmdline, "help")) { cmd_help(args); return; }
    if (!strcmp(cmdline, "echo")) { cmd_echo(args); return; }
    if (!strcmp(cmdline, "clear")) { cmd_clear(args); return; }
    if (!strcmp(cmdline, "ls")) { cmd_ls(args); return; }
    if (!strcmp(cmdline, "cat")) { cmd_cat(args); return; }
    if (!strcmp(cmdline, "halt")) { cmd_halt(args); return; }
    if (!strcmp(cmdline, "exit")) { cmd_exit(args); return; }

    // Command not found
    print("Unknown command: ");
    print(cmdline);
    print("\nType 'help' for available commands.\n");

}

// Main shell loop
void main() {
    char linebuf[LB_SIZE];
    size_t pos = 0;
    
    print("\n=== Shell ===\n");
    print("Type 'help' for available commands.\n\n");
    
    while (1) {
        print("$ ");
        pos = 0;
        memset(linebuf, 0, LB_SIZE);
        
        // Read line
        while (1) {
            uint64_t result = sys_terminal_read();
            
            if (result == (uint64_t)-1) {
                continue;  // No data available, keep trying
            }
            
            char c = (char)(result & 0xFF);
            
            // Handle newline/enter
            if (c == '\n' || c == '\r') {
                //print("\n");
                linebuf[pos] = '\0';
                break;
            }
            
            // Handle backspace
            if (c == '\b' || c == 127) {
                if (pos > 0) {
                    pos--;
                    //print("\b \b");  // Erase character on screen
                }
                continue;
            }
            
            // Add character to buffer
            if (pos < LB_SIZE - 1 && c >= 32 && c < 127) {
                linebuf[pos++] = c;
                sys_serial_putc(c);  // Echo character
            }
        }
        
        
        // Execute command
        execute_command(linebuf);
    }
}
