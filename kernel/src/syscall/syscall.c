#include "../iodebug.h"
#include "../memory.h"
#include <lai/helpers/pm.h>
#include "../util/fb.h"
#include "../timer.h"
#include "../drivers/cmos/rtc.h"
#include "../ff16/source/ff.h"
#include "../fs/fs.h"
#include "../mm/pmm.h"
#include "../panic/panic.h"
#include "../drivers/ahci/ahci.h"
#include "../errno.h"
#include "../userspace/enter.h"
#include "../elf/elf.h"
#include "../buffer/buffer.h"
#include "../mm/liballoc.h"
#include "../thread/thread.h"
#include "../userspace/exec.h"
#include "../userspace/enter.h"
#include "syscall.h"

extern void isr_stub_105(void);

size_t strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len]) len++;
    return len;
}

void init_syscall() {
	set_idt_entry(0x69, isr_stub_105, 3);
	task_exit();
}

/* copy_from_user – copy `len` bytes from a user‑mode pointer to kernel space.
 * Returns the kernel virtual address of the allocated buffer (0 on failure). */
// todo: security
static inline uint64_t copy_from_user(const void *src, size_t len)
{
    /* Allocate whole pages that can hold `len` bytes.  */
    void *k_mem = palloc((len + PAGE_SIZE - 1) / PAGE_SIZE, true);
    if (!k_mem) { panik_no_mem(); return 0; }                 /* allocation failed */

    memcpy(k_mem, src, len);    /* copy the user data */

    /* Convert the kernel pointer to a uint64_t before returning. */
    return (uint64_t)(uintptr_t)k_mem;
}

/* Copies *len* bytes from kernel buffer *k_mem* into user space address *user_dst*.
   Returns 0 on success, -1 if any byte of the destination lies outside the user address space. */
static inline int copy_to_user(uint64_t k_mem, void *user_dst, size_t len)
{
    /*
    if (!is_valid_user_ptr(user_dst, len))
        return -1;                  
*/
    memcpy(user_dst, (void *)k_mem, len);
    return 0;
}

static void release_stack(uintptr_t base) {
    if (base >= VM_HIGHER_HALF) {
        base -= VM_HIGHER_HALF;
        pfree((void*)base, STACK_PAGES);
    } else {
        pfree((void*)base, STACK_PAGES);
    }
}

char* path;
int open_flags;
mode_t mode;
int fuckyou;

void openfile() {
    // open
    serial_puts("path(syscall): ");
    serial_puts(path);
    fuckyou = open(path, open_flags, mode);
    task_exit();
}

cpu_status_t* syscall_handler(cpu_status_t* regs) {
    serial_puts("\ngot syscall: ");
    serial_puthex(regs->rax);
    serial_puts("!");
    // rax is syscall number
    switch (regs->rax) {
        case 0:
            // read
            int fd = regs->rdi; // file descriptor
            char* buf = (char*)regs->rsi; // buffer to read into
            size_t count = regs->rdx; // number of bytes to read
            read(fd, buf, count);
            regs->rax = count; // number of bytes read
            break;
        case 1:
            // write
            fd = regs->rdi; // file descriptor
            count = regs->rdx; // number of bytes to write
            const char* buff = (const char*)copy_from_user((const void*)regs->rsi, count); // buffer to write from
            write(fd, buff, count);
            regs->rax = count; // number of bytes written
            break;
        case 2:
            // open
            open_flags = regs->rsi; // flags for opening
            mode = 0; // mode for opening
            path = (char*)copy_from_user((const void*)regs->rdi, 12); // path to file
            //open(path, open_flags, mode);
            create_task(openfile);
            yield();
            regs->rax = fuckyou;
            pfree((void*)path, (12 + PAGE_SIZE - 1) / PAGE_SIZE);
            break;
        case 3:
            // close
            fd = regs->rdi; // file descriptor to close
            regs->rax = close(fd);
            break;
        case 4:
            // map memory
            uint64_t va = (uint64_t)regs->rdi; // virtual address
            uint64_t pa = (uint64_t)regs->rsi; // physical address
            uint64_t flags = (uint64_t)regs->rdx; // flags

            map_page(read_cr3(), va, pa, flags);
            break;
        case 5:
            // serial_puts
            serial_puts((const char*)regs->rdi);
            break;
        case 6:
            // serial_puthex
            serial_puthex(regs->rdi);
            break;
        case 7:
            // serial_putc
            serial_putc((char)regs->rdi);
            break;
        case 8:
            // print
            flanterm_write(flanterm_get_ctx(), (const char*)regs->rdi, regs->rsi);
            break;
        case 9:
            // exit current task
            task_exit();
            break;
        case 10:
            // get fb usermode address
            regs->rax = get_userland_fb_addr();
            regs->rbx = get_fb_size();
            break;
        case 11: // deprecated, due removal
            // read first char from read-only terminal buffer (lb_read)
            linebuf_t* in = fetch_linebuffer();
            regs->rax = lb_read(in);
            break;
        case 12:
            // acpi sleep
            lai_enter_sleep(regs->rdi);
            break;
        case 13:
            // opendir
            regs->rax = opendir((char*)copy_from_user((const void*)regs->rdi, 64));
            break;
        case 14:
            // readdir
            fd = regs->rdi;          // user dirfd
            void* user_fno = (void*)regs->rsi;  // user-space pointer to FILINFO

            FILINFO fno;
            long ret = readdir(fd, &fno);  // kernel-space copy

            copy_to_user((uint64_t)&fno, user_fno, sizeof(FILINFO));

            regs->rax = ret;  // return status to user
            break;
        case 15:
            // read data from RTC
            rtc_t rtc = read_rtc();
            void* user_rtc = (void*)regs->rdi;  
            copy_to_user((uint64_t)&rtc, user_rtc, sizeof(rtc_t));
            break;
        case 16:
            // exec 
            regs->iret_rip = (uint64_t)load_program(regs->rdi);
            break;
        case 17:
            // get fb info (deprecated)
            struct fb_info info = get_fb_info();
            void* user_fb_info = (void*)regs->rsi;  // user-space pointer to fb_info
            regs->rax = copy_to_user((uint64_t)&info, user_fb_info, sizeof(info));
            break;
        case 18:
            // sync to disk
            regs->rax = (uint64_t)sync();
            break;
        case 19:
            // fstat
            regs->rax = fstat(fd, (struct stat*)copy_from_user((const void*)regs->rdi, sizeof(struct stat)));
            break;
        case 20: {
            // execve
            const char *path = (const char *)regs->rdi;
            char **argv = (char **)regs->rsi;
            char **envp = (char **)regs->rdx;
            
            // Count and copy argv strings to kernel memory FIRST
            int argc = 0;
            while (argv && argv[argc]) argc++;
            
            int envc = 0;
            while (envp && envp[envc]) envc++;
            
            // Allocate kernel buffers for the strings
            char *argv_copy[argc + 1];
            for (int i = 0; i < argc; i++) {
                size_t len = strlenn(argv[i]) + 1;
                argv_copy[i] = malloc(len);  // or however you allocate kernel memory
                memcpy(argv_copy[i], argv[i], len);
            }
            argv_copy[argc] = NULL;
            
            char *envp_copy[envc + 1];
            for (int i = 0; i < envc; i++) {
                size_t len = strlenn(envp[i]) + 1;
                envp_copy[i] = malloc(len);
                memcpy(envp_copy[i], envp[i], len);
            }
            envp_copy[envc] = NULL;
            
            fd = open(path, O_RDONLY, 0);
            if (fd < 0) {
                // cleanup argv_copy and envp_copy
                regs->rax = fd;
                break;
            }
            
            uint64_t entry = load_program(fd);
            close(fd);
            
            if (!entry) {
                // cleanup argv_copy and envp_copy
                regs->rax = -ENOEXEC;
                break;
            }
            
            // allocate new stack
            uintptr_t stack = (uintptr_t)palloc(STACK_PAGES, false);
            if (stack == 0) {
                unload_elf((void*)entry);
                regs->rax = -ENOMEM;
                break;
            }
            
            // release old stack if it still exists
            if (regs->iret_rsp != 0)
                release_stack(regs->iret_rsp);

            // map the new stack
            uint64_t stack_top = stack + STACK_SIZE - 8;
            map_len(
                read_cr3(),
                stack,
                stack,
                PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE,
                STACK_SIZE
            );
            
            // setup stack with the copied strings
            stack_top = setup_stack(
                (uintptr_t *)stack_top,
                argv_copy,
                envp_copy
            );

            // reset heap
            task_t *t = get_current_task();
            t->heap_end = t->heap_start;
            
            // Free the kernel copies after setup_stack is done
            for (int i = 0; i < argc; i++) free(argv_copy[i]);
            for (int i = 0; i < envc; i++) free(envp_copy[i]);
            
            // set special regs
            regs->iret_rsp = (uint64_t)stack_top;
            regs->iret_rip = entry;
            regs->rax = 0;
            // set the GPRs
            regs->rdi = argc;                           // argc = 2
            regs->rsi = (uint64_t)argv[0];       // &argv[0]
            regs->rdx = (uint64_t)envp[0]; // &envp[0]
            regs->rbp = 0;
            break;
        }
        case 21:
            regs->rax = (uint64_t)sbrk(regs->rdi);
            break;
        default:
            regs->rax = E_NO_SYSCALL;
            break;
    }

    return regs;
}
