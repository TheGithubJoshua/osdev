#pragma once
#include <stdint.h>

typedef struct cpu_status_t {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t vector_number;
    uint64_t error_code;

    uint64_t iret_rip;
    uint64_t iret_cs;
    uint64_t iret_flags;
    uint64_t iret_rsp;
    uint64_t iret_ss;
} cpu_status_t;

void idt_init(void);
void irq_unmask_all();
void irq_remap();
void irq_handler(cpu_status_t* cpu_status_t);
void init_pit(uint32_t frequency);
extern void switch_from_irq(cpu_status_t *cpu);
typedef void (*irq_handler_t)(void);
void register_irq_handler(uint8_t irq, irq_handler_t handler);
void sata_irq_handler();
void set_idt_entry(uint8_t vector, void* handler, uint8_t dpl);
cpu_status_t* syscall_handler(cpu_status_t* regs);