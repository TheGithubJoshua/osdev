#pragma once

#include <stdint.h>

#define TSS_SELECTOR (5 * 8)  // GDT selectors are index * 8
#define STACK_SIZE 0x4000

#define USER_CODE_VADDR 0x400000

#define KERNEL_STACK_SIZE 0x4000 // 16KB

// Declare gdtr but don't define it here
/*typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;
*/
//extern gdtr_t gdtr;

struct tss_entry_struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_bitmap_offset;
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;

extern uintptr_t kernel_stack_top;
extern uint64_t elf_size;

void enter_userspace();
void test_user_function();