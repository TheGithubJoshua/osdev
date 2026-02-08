#include "thread.h"
#include "../memory.h"
#include "../elf/elf.h"
#include "../errno.h"
#include "../iodebug.h"
#include "../userspace/enter.h"
#include "../syscall/syscall.h"
#include "../mm/liballoc.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"

uint64_t stack_top2;
uint64_t user_code_vaddr2;
uintptr_t incr;
extern void jump_usermode2();
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

static inline uintptr_t read_rip(void)
{
    uintptr_t rip;
    __asm__ volatile (
        "lea (%%rip), %0"
        : "=r"(rip)
        :
        : "memory"
    );
    return rip;
}

static void release_stack(uintptr_t base) {
    if (base >= VM_HIGHER_HALF) {
        base -= VM_HIGHER_HALF;
        pfree((void*)base, STACK_PAGES);
    } else {
        pfree((void*)base, STACK_PAGES);
    }
}

/*void fork_trampoline(void) {
	// child is now running
	serial_puts("child is now running!");
	stack_top2 = read_rsp() + 5*8;
	user_code_vaddr2 = get_current_task()->image_base+incr;
	jump_usermode2();
	//return;
}*/
__attribute__((naked))
void fork_trampoline(void) {
	// child is now running
    __asm__ volatile (
        "xor %rax, %rax\n\t"    // fork() returns 0 in child
        "movq %rsp, %rdi\n\t" // preserve saved rip
        "pushq $0x23\n\t"       // push SS (RPL=3)
        "pushq $0x202\n\t"      // push RFLAGS, IF=1, default flags
        "pushq $0x1B\n\t"       // push CS, user code segment (RPL=3)
        "movq %rdi, %rsp\n\t" // restore saved rip to the top
        "ret\n\t"
    );
}

pid_t do_fork(uintptr_t rip, uintptr_t rsp) {
	incr = rip - get_current_task()->image_base;
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

	// map the new stack
	map_len(
	    read_cr3(),
	    stack,
	    stack,
	    PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE,
	    STACK_SIZE
	);

    uint64_t parent_top = parent->stack_base + STACK_SIZE - 8;
    //child->stack_base = stack;
    release_stack(child->stack_base); // release create_task stack so we can use custom one
    child->stack_base = stack;
    uint64_t child_top = child->stack_base + STACK_SIZE - 8;
    size_t used = parent_top - (size_t)rsp;
	child->rsp = (uint64_t*)(child_top - used); // set child rsp

	// copy the new stack
	memcpy(child->rsp, (void*)rsp, used);

	// fake return address → fork_tramoline(void)
	STACK_PUSH(child->rsp, (uint64_t)fork_trampoline);
    
    // allocate and map process image
    uintptr_t image_pages = PAGE_PAGES(parent->image_size);
    uintptr_t image = (uintptr_t)palloc(image_pages, false);
    if (image == 0) {
    	return -ENOMEM;
    }
    
    uintptr_t image_virt = find_address(parent->image_size); // to future me: #PF (prot-violation) occurs on 2nd or so intstruction into usermode. -_-
    map_len(
        read_cr3(),
        image_virt,
        image,
        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER,
        parent->image_size
    );

    child->image_base = image_virt;
    child->image_size = parent->image_size;

	memcpy((void*)child->image_base, (void*)parent->image_base, parent->image_size); // copy proc image
	// syscall handler should set rip later
	
	// executing process should be parent
	//child->state = TASK_READY;
	return child->pid;
}