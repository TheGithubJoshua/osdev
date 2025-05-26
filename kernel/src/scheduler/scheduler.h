// -------------------------
// Task Control Block (TCB)
// -------------------------
#include <stddef.h>
#include <stdint.h>

typedef struct task_t {
    uint64_t *rsp;         // Saved kernel stack pointer (RSP) for this task
    struct task_t *next;   // Next task in the circular ready queue
    uint8_t state;         // Task state (e.g., RUNNING or READY)
    uint64_t wake_time;    // 
} task_t;

enum { TASK_RUNNING, TASK_READY, DEAD, SLEEPING };

typedef struct thread_control_block thread_control_block_t;
void initialise_multitasking(void);
extern void switch_to_task(task_t *next);
extern task_t *current_task;
extern void yield(void);
task_t *create_task(void (*entry)(void));
extern void initial_switch();
void boot_task();
void task_exit();
void task_sleep(task_t* task, size_t sleep);

extern volatile bool multitasking_initialized;

/*extern void switch_to_task(thread_control_block_t* next_thread);
extern thread_control_block_t* current_task_TCB;
*/