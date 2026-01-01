#include "thread.h"
#include "../memory.h"
#include "../elf/elf.h"
#include "../errno.h"
#include "../iodebug.h"
#include "../userspace/enter.h"
#include "../mm/liballoc.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"

static inline uintptr_t read_rsp(void) {
    uintptr_t rsp;
    __asm__ volatile (
        "mov %%rsp, %0"
        : "=r"(rsp)
        :
        : "memory"
    );
    return rsp;
}

/*void fork_trampoline(void) {
	// child is now running
	serial_puts("child is now running!");
	return;
}*/
__attribute__((naked))
void fork_trampoline(void) {
	// child is now running
    __asm__ volatile (
        "lea 0x40(%rsp), %rsp\n\t"
        "xor %rax, %rax\n\t"    // fork() returns 0 in child
        "ret\n\t"
    );
}


pid_t do_fork(uintptr_t rsp) {
	rsp = read_rsp();
	// get parent
	task_t *parent = get_current_task();
	// allocate the TCB
	/*task_t *child = vmm_alloc(sizeof(*child), VM_FLAG_USER | VM_FLAG_WRITE, NULL);
	memset(child, 0, sizeof(*child)); // zero the child tcb
	if (!child) return -ENOMEM;
	//t->state = TASK_READY;
    
    // assign pid
	int pid = get_free_pid();
	if (pid == -1) {
	    return -EAGAIN;
	}

	// copy over the TCB slots
	//memcpy(child, parent, sizeof(task_t));

	// set pid
	child->pid = pid;*/
	task_t *child = create_task(fork_trampoline);
	child->signal = 0; // clear fucking signals for some reason

	// allocate child stack
	uintptr_t stack = (uintptr_t)palloc(STACK_PAGES, false);
	if (stack == 0) {
		return -ENOMEM;
	}
	extern uintptr_t kernel_stack_top;

	uint64_t parent_top = kernel_stack_top;
	child->stack_base = stack;
	uint64_t child_top = stack + STACK_SIZE - 8;
	size_t used = parent_top - (size_t)rsp;

	// map the new stack
	map_len(
	    read_cr3(),
	    stack,
	    stack,
	    PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE,
	    STACK_SIZE
	);

	child->rsp = (uint64_t*)(child_top - used); // set child rsp

	// copy the new stack
	memcpy(child->rsp, (void*)rsp, used);

	// fake return address → fork_tramoline(void)
	((uint64_t *)child->rsp)[0] = (uint64_t)fork_trampoline;
    
    // allocate and map process image
    uintptr_t image = (uintptr_t)palloc(STACK_PAGES, false);
    if (image == 0) {
    	return -ENOMEM;
    }

    map_len(
        read_cr3(),
        image,
        image,
        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER,
        STACK_SIZE
    );
    child->image_base = image;

	memcpy((void*)child->image_base, (void*)parent->image_base, parent->image_size); // copy proc image
	// syscall handler should set rip later
	
	// executing process should be parent
	//child->state = TASK_READY;
	return child->pid;
}