#include "../iodebug.h"
#include "../drivers/fat/fat.h"
#include "../memory.h"
#include "../util/fb.h"
#include "../thread/thread.h"
#include "syscall.h"

extern void isr_stub_105(void);

void init_syscall() {
	set_idt_entry(0x69, isr_stub_105, 3);
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
            serial_puts("not implemented!");
            break;
        case 1:
            // write
            break;
        case 2:
            // open
            break;
        case 3:
            // close
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
        default:
            regs->rax = E_NO_SYSCALL;
            break;
    }

    return regs;
}