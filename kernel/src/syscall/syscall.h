#include "../interrupts/interrupts.h"

#define E_NO_SYSCALL 0xBADCA11
#define SYSCALL_SUCCESS -1

#define STACK_PUSH(PTR, VAL) *(--(PTR)) = VAL
#define AUXVAL(PTR, TAG, VAL)   \
    STACK_PUSH(PTR, VAL);       \
    STACK_PUSH(PTR, TAG);

void init_syscall();
cpu_status_t* syscall_handler(cpu_status_t* regs);
void openfile();