#include <stdint.h>
#include "../mm/pmm.h"
#include "../util/fb.h"
#include "thread.h"
#include "../iodebug.h"
#include "../timer.h"

volatile bool multitasking_initialized = false;

// Pointers to the current task (running) and head of task list.
task_t *current_task = NULL;

// Forward declaration of the second task’s entry function.
void second_task_function(void);
task_t *create_task(void (*entry)(void));
void task_exit();
void task_sleep(task_t* task, size_t sleep);

extern task_t *current_task;

void debug_task_list(void) {
    serial_puts("Task list:\n");

    task_t *start = current_task;
    task_t *t = start;
    do {
        serial_puts("  Task ");
        serial_puthex((uintptr_t)t);
        serial_puts("  [rsp: ");
        serial_puthex((uintptr_t)t->rsp);
        serial_puts("]  state: ");
        serial_puthex(t->state);
        serial_puts("\n");
        t = t->next;
    } while (t != start);
}

void boot_task() {
    //serial_puts("Boot task: initializing multitasking\n");

    // Start other tasks (e.g. task_a, task_b...)
    //initialise_multitasking(); // creates tasks and enables round-robin

    // After this, the timer IRQ will start switching between tasks
    while (1) {
        serial_puts("Boot task running again!\n");
        timer_wait(10); // Sleep so others can run
    }
}

void task_d(void) {
    //while (1) {
        serial_puts("Task D running\n");
        //yield();
        timer_wait(10);
        serial_puts("Returned to D\n");
        task_exit();
    //}
}

void task_c(void) {
    //while (1) {
        serial_puts("Task C running\n");
        //yield();
        //timer_wait(1);
        //serial_puts("Returned to C\n");
        task_exit();
    //}
}

void task_b(void) {
   // while (1) {
        serial_puts("Task B running\n");
        //yield();
        //timer_wait(10);
        //task_sleep(current_task, 999999999999999999);
        //serial_puts("Returned to B\n");
        //timer_wait(1);
        task_exit();
    //}
}
void debug_task_list(void);

void task_a(void) {
    __asm__ __volatile__ ("sti");
   /* task_t *second = palloc(sizeof(task_t), true);
    second->state = TASK_READY;

    // 2a) Allocate a kernel stack for the second task.
    const size_t STACK_SIZE = 8192;  // 8 KB stack (adjust as needed)
    uint64_t *stack_bottom = palloc(STACK_SIZE * sizeof(uint64_t), true);
    // The stack grows down, so initial RSP is at top of allocated block.
    second->rsp = stack_bottom + STACK_SIZE;

    // 2b) Set up an initial stack frame for the new task.
    // The new task will resume by "ret" into second_task_function().
    *--second->rsp = (uint64_t)task_b; 
    // (We could also push fake RBP, etc., if needed by the assembly context switch.)

    // 3) Link the new task into a circular list with the current task:contentReference[oaicite:5]{index=5}.
    current_task->next = second;
    second->next = current_task;
    */
debug_task_list();
    //switch_to_task(second);
        while (1) {
        serial_puts("Task A running\n");
        //current_task->state = TASK_RUNNING;
        //yield();
        serial_puts("Returned to A\n");
        timer_wait(10);
        serial_puts("next bit of task a");
    }
}

#define STACK_SIZE_def 8192

/// Create a new task that starts at `entry()`
task_t *create_task(void (*entry)(void)) {
    asm volatile("cli");

    // 1) Allocate the TCB
    task_t *t = palloc((sizeof(*t) + PAGE_SIZE - 1) / PAGE_SIZE, true);
    if (!t) return NULL;
    t->state = TASK_READY;

    // 2) Allocate & build the stack
    #define STACK_WORDS (8192/8)
    serial_puts("stack :\n");
    uint64_t *stack = palloc((STACK_WORDS * sizeof *stack + PAGE_SIZE - 1) / PAGE_SIZE, true);
    serial_puthex((uintptr_t)stack);
    if (!stack) { pfree(t, true); return NULL; }
    uint64_t *rsp = stack + STACK_WORDS;


    // 2a) push dummy callee-saved registers for yield()
    *--rsp = 0;  // r15
    *--rsp = 0;  // r14
    *--rsp = 0;  // r13
    *--rsp = 0;  // r12
    *--rsp = 0;  // rbx
    *--rsp = 0;  // rbp

    // 2b) fake return address → entry()
    *--rsp = (uint64_t)entry;

    t->rsp = rsp;
    rsp = (uint64_t *)((uintptr_t)rsp & ~0xF);

    // 3) splice into the existing circle
    if (!current_task) {
    t->next = t;
    current_task = t;
} else {
    // Find the last task
    task_t *last = current_task;
    while (last->next != current_task)
        last = last->next;

    last->next = t;
    t->next = current_task;
}
return t;
}

void task_free() {
    task_t* dead = current_task->next;

    // Don't delete if it's the only task left
    if (dead == current_task) return;

    if (dead->state == DEAD) {
        // Find the previous task to the one being deleted
        task_t* prev = current_task;
        while (prev->next != dead) {
            prev = prev->next;
        }

        // Unlink dead task
        prev->next = dead->next;

        // If current_task was pointing to dead, advance it
        if (current_task == dead) {
            current_task = dead->next;
        }

        // Free stack and TCB
        pfree((void*)dead->rsp, true);  // Only if stack is page-allocated
        pfree((void*)dead, true);

        debug_task_list();
    }
}

void task_exit() {
    asm ("sti");
    current_task->state = DEAD;
    while(true);
}

void task_sleep(task_t* task, size_t sleep) {
    flanterm_write(flanterm_get_ctx(), "\033[33m", 5);
    flanterm_write(flanterm_get_ctx(), "[KERNEL][WARN] task_sleep() is currently broken. \n", 50);
    flanterm_write(flanterm_get_ctx(), "\033[0m", 5);
    task->state = SLEEPING;
    task->wake_time = uptime() + sleep;
    yield();
}

// initialise_multitasking: Set up the initial (boot) task and one additional task.
void initialise_multitasking(void) {
    //asm volatile("cli");

    // 1) Create boot task TCB and store current state
    current_task = palloc((sizeof(task_t) + PAGE_SIZE - 1) / PAGE_SIZE, true);
    current_task->state = TASK_RUNNING;

    // Save the current stack pointer into boot task
    __asm__ volatile("mov %%rsp, %0" : "=r"(current_task->rsp));

    // Initialize circular list with only the boot task for now
    current_task->next = current_task;

    // 2) Create additional tasks — these will be inserted after current_task
    create_task(task_a);
    create_task(task_b);
    create_task(task_c);
    create_task(task_d);

    debug_task_list();

    //asm volatile("sti");

    // 3) Start round-robin loop: yield to next task
    serial_puts("Switching to next task...\n");
    multitasking_initialized = true;
    //boot_task();
    switch_to_task(current_task->next);
    //yield();  // Will save boot task's state and switch to the next
    // If yield ever returns (e.g., when boot task runs again)
    while (1) {
        serial_puts("Boot task running again!\n");
        timer_wait(50);
        //yield();
    }
    
}
