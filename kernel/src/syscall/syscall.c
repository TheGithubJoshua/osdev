#include "../interrupts/interrupts.h"
#include "../thread/thread.h"
#include "syscall.h"

extern void isr_stub_105(void);

void init_syscall() {
	set_idt_entry(0x69, isr_stub_105, 3);
	task_exit();
}