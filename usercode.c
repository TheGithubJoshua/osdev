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
#define FA_READ             0x01

typedef unsigned long uint64_t;
typedef unsigned long size_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
//typedef int bool;
#define true 1
#define false 0

#define AM_DIR  0x10    /* Directory */

typedef struct rtc_t {
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned int year;

    unsigned char century;
    unsigned char last_second;
    unsigned char last_minute;
    unsigned char last_hour;
    unsigned char last_day;
    unsigned char last_month;
    unsigned char last_year;
    unsigned char last_century;
    unsigned char registerB;
} rtc_t;

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

static inline void sys_rtc(rtc_t* rtc) {
    asm volatile(
        "int $0x69"
        :
        : "a"((uint64_t)15), "D"(rtc)
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

static inline uint64_t sys_sync(void) {
    uint64_t ret;
    asm volatile(
        "int $0x69\n\t"
        : "=a"(ret)
        : "a"((uint64_t)18)
        : "memory"
    );
    return ret;
}

/* return fd (or negative errno) */
static inline long sys_open(const char *name, unsigned long flags, unsigned long mode) {
    long ret;
    asm volatile(
        "mov %[num], %%rax\n\t"
        "mov %[p],   %%rdi\n\t"
        "mov %[f],   %%rsi\n\t"
        "mov %[m],   %%rdx\n\t"
        "int $0x69\n\t"
        "mov %%rax, %[ret]\n\t"
        : [ret] "=r"(ret)
        : [num] "r"((unsigned long)2),
          [p]   "r"(name),
          [f]   "r"(flags),
          [m]   "r"(mode)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    return ret;
}


/* File/directory information structure (FILINFO) */

typedef struct {
    unsigned int fsize;          /* File size (invalid for directory) */
    uint16_t    fdate;          /* Date of file modification or directory creation */
    uint16_t    ftime;          /* Time of file modification or directory creation */
    uint8_t    fattrib;        /* Object attribute */
    char   fname[12 + 1];  /* Object name */
} FILINFO;

static inline long sys_opendir(const char *name) {
    long ret;
    asm volatile(
        "mov %[num], %%rax\n\t"
        "mov %[p],   %%rdi\n\t"
        "int $0x69\n\t"
        "mov %%rax, %[ret]\n\t"
        : [ret] "=r"(ret)
        : [num] "r"((unsigned long)13),
          [p]   "r"(name)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    return ret;
}

static inline void sys_close(int fd) {
    asm volatile(
        "int $0x69"
        :
        : "a"((uint64_t)3), "D"(fd)
        : "memory"
    );
}

/* return bytes read (or negative errno) */
static inline long sys_read(int fd, char *buf, size_t size) {
    long ret;
    asm volatile(
        "mov %[num], %%rax\n\t"
        "mov %[p],   %%rdi\n\t"
        "mov %[s],   %%rsi\n\t"
        "mov %[d],   %%rdx\n\t"
        "int $0x69\n\t"
        "mov %%rax, %[ret]\n\t"
        : [ret] "=r"(ret)
        : [num] "r"((unsigned long)0),
          [p]   "r"((uint64_t)fd),
          [s]   "r"(buf),
          [d]   "r"(size)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_write(int fd, const char *buf, size_t size) {
    long ret;
    asm volatile (
        "mov %[num], %%rax\n\t"
        "mov %[a1],  %%rdi\n\t"
        "mov %[a2],  %%rsi\n\t"
        "mov %[a3],  %%rdx\n\t"
        "int $0x69\n\t"
        "mov %%rax, %[ret]\n\t"
        : [ret] "=r" (ret)
        : [num] "r" ((unsigned long)1),          /* syscall number */
          [a1]  "r" ((unsigned long)fd),
          [a2]  "r" ((unsigned long)buf),
          [a3]  "r" ((unsigned long)size)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_readdir(int fd, FILINFO* fno) {
    long ret;
    asm volatile(
        "mov %[num], %%rax\n\t"
        "mov %[p], %%rdi\n\t"
        "mov %[f], %%rsi\n\t"
        "int $0x69\n\t"
        "mov %%rax, %[ret]\n\t"
        : [ret] "=r"(ret)
        : [num] "r"((unsigned long)14),
          [p]   "r"((unsigned long)fd),
          [f]   "r"((unsigned long)fno)
        : "rax", "rdi", "rcx", "r11", "memory"
    );
    return ret;
}

static inline void sys_exec(int fd) {
    asm volatile(
        "int $0x69"
        :
        : "a"((uint64_t)16), "D"(fd)
        : "memory"
    );
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
    print("  clock   - Display current date and time\n");
    print("  halt    - Shutdown the system\n");
    print("  exec    - Executes a program on disk\n");
    print("  exit    - Exit the shell\n");
}

void cmd_echo(const char *args) {
    print(args);
    print("\n");
}

void cmd_exec(const char *args) {
if ((args != 0) && (args[0] != '\0')) {
    long fd = sys_open(args, 0, 0);
    if (fd >= 0) {
    sys_exec(fd);
    sys_close(fd);
} else {
    print("exec: ");
    print(args);
    print(": ");
    print("No such file or directory");
    print("\n");
}
}
}

void cmd_clear(const char *args) {
    print("\033[2J\033[H");  // ANSI escape codes
}

void cmd_clock(const char *args) {
    rtc_t rtc;
    sys_rtc(&rtc);

    // Format YYYY-MM-DDTHH:MM:SSZ
    // Format each component as two-digit strings with leading zeros if necessary
    char hr_str[3];
    if (rtc.hour < 10) {
        hr_str[0] = '0';
    } else {
        hr_str[0] = rtc.hour / 10 + '0';
    }
    hr_str[1] = rtc.hour % 10 + '0';
    hr_str[2] = '\0';

    char min_str[3];
    if (rtc.minute < 10) {
        min_str[0] = '0';
    } else {
        min_str[0] = rtc.minute / 10 + '0';
    }
    min_str[1] = rtc.minute % 10 + '0';
    min_str[2] = '\0';

    char sec_str[3];
    if (rtc.second < 10) {
        sec_str[0] = '0';
    } else {
        sec_str[0] = rtc.second / 10 + '0';
    }
    sec_str[1] = rtc.second % 10 + '0';
    sec_str[2] = '\0';

    char day_str[3];
    if (rtc.day < 10) {
        day_str[0] = '0';
    } else {
        day_str[0] = rtc.day / 10 + '0';
    }
    day_str[1] = rtc.day % 10 + '0';
    day_str[2] = '\0';

    char month_str[3];
    if (rtc.month < 10) {
        month_str[0] = '0';
    } else {
        month_str[0] = rtc.month / 10 + '0';
    }
    month_str[1] = rtc.month % 10 + '0';
    month_str[2] = '\0';

    // Calculate the full year
    int full_year = (rtc.century * 100) + rtc.year;
    char year_str[5];
    year_str[0] = (full_year / 1000) + '0';
    year_str[1] = (full_year % 1000 / 100) + '0';
    year_str[2] = (full_year % 100 / 10) + '0';
    year_str[3] = full_year % 10 + '0';
    year_str[4] = '\0';

    // Manually construct the time string (in ISO 8601 ofc)
    char time_str[26];
    int index = 0;

    // Add year and hyphen
    time_str[index++] = year_str[0];
    time_str[index++] = year_str[1];
    time_str[index++] = year_str[2];
    time_str[index++] = year_str[3];
    time_str[index++] = '-';
    
    // Add month and hyphen
    time_str[index++] = month_str[0];
    time_str[index++] = month_str[1];
    time_str[index++] = '-';
    index += 1;

    // Add day and T
    time_str[index++] = day_str[0];
    time_str[index++] = day_str[1];
    time_str[index++] = 'T';
    index += 1;

    // Add hours and colon
    time_str[index++] = hr_str[0];
    time_str[index++] = hr_str[1];
    time_str[index++] = ':';
    index += 1; // Increment after adding each character

    // Add minutes and colon
    time_str[index++] = min_str[0];
    time_str[index++] = min_str[1];
    time_str[index++] = ':';
    index += 1;

    // Add seconds and Z
    time_str[index++] = sec_str[0];
    time_str[index++] = sec_str[1];
    time_str[index++] = 'Z';
    index += 1;

    // Ensure null termination
    time_str[index] = '\0';

    print("The current date and time is: ");
    sys_print(time_str, 25);
    print("\n");
}

void cmd_ls(const char *args) {
    long dir_fd = sys_opendir(args);
    if (dir_fd < 0) {
        print("Failed to open ");
        print(args);
        print("\n");
        return;
    }

    FILINFO fno;
    long ret;
    int nfile = 0, ndir = 0;

    for (;;) {
        ret = sys_readdir(dir_fd, &fno);
        if (ret < 0 || fno.fname[0] == '\0') break;

        if (fno.fattrib & AM_DIR) {
            print(" <DIR> ");
            print(fno.fname);
            print(" ");
            ndir++;
        } else {
     //       print_u32(fno.fsize);
    //        print(" ");
            print(fno.fname);
            print(" ");
            nfile++;
        }
    }
    
    print("\n");
    sys_close(dir_fd);

    //print_u32(ndir);
    //print(" dirs, ");
    //print_u32(nfile);
    //print(" files.\n");
}
#define O_RDONLY 0
void cmd_cat(const char *args) {
    if (!args) {
        sys_serial_puts("cat: no filename\n");
        return;
    }

    sys_serial_puts("catting file: ");
    sys_serial_puts(args);

    unsigned long flags = O_RDONLY;
    unsigned long mode = 0; /* if not used, pass 0 */

    long fd = sys_open(args, flags, mode);
    if (fd < 0) {
        sys_serial_puts("open failed: ");
        return;
    }

    char buf[256];
    size_t want = sizeof(buf);

    long got = sys_read((int)fd, buf, want);
    if (got < 0) {
        sys_serial_puts("read failed\n");
        return;
    }

    /* ensure NUL for printing */
    size_t n = (size_t)got;
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    buf[n] = '\0';

    sys_serial_puts("usermode read: ");
    sys_serial_puts(buf);
    print(buf);
    sys_close((int)fd);
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
    if (!strcmp(cmdline, "clock")) { cmd_clock(args); return; }
    if (!strcmp(cmdline, "halt")) { cmd_halt(args); return; }
    if (!strcmp(cmdline, "exec")) { cmd_exec(args); return; }
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
    
    print("\n=== Monolith64 Shell v0.1 ===\n");
    print("Type 'help' for available commands.\n\n");
    
    while (1) {
        print("> ");
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
