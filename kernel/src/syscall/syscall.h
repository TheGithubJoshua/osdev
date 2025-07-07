#include "../interrupts/interrupts.h"

#define E_NO_SYSCALL 0xBADCA11

void init_syscall();
cpu_status_t* syscall_handler(cpu_status_t* regs);