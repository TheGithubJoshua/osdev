#include "../iodebug.h"
#include "../drivers/fat/fat.h"
#include "../memory.h"
#include <lai/helpers/pm.h>
#include "../util/fb.h"
#include "../timer.h"
#include "../ff16/source/ff.h"
#include "../drivers/fat/fat.h"
#include "../fs/fs.h"
#include "../mm/pmm.h"
#include "../drivers/ahci/ahci.h"
#include "../userspace/enter.h"
#include "../buffer/buffer.h"
#include "../thread/thread.h"
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
    if (!k_mem)                 /* allocation failed */
        return 0;

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
            break;
        case 2:
            // open
            open_flags = regs->rsi; // flags for opening
            mode = (mode_t)regs->rdx; // mode for opening
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
            close(fd);
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
        case 11:
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
        default:
            regs->rax = E_NO_SYSCALL;
            break;
    }

    return regs;
}
