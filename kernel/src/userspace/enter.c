#include "memory.h"
#include "../mm/pmm.h"
#include "../util/fb.h"
#include "../elf/elf.h"
#include "../drivers/fat/fat.h"
#include "../iodebug.h"
#include "enter.h"
#include <stdbool.h>

extern void jump_usermode();
tss_entry_t tss_entry;
uint64_t stack_top;
uint64_t user_code_vaddr;
//gdtr_t gdtr;

unsigned char loop[2] = { 0xEB, 0xFE };

void enter_userspace(const char *fn) {
    flanterm_write(flanterm_get_ctx(), "\033[32m", 5);
    flanterm_write(flanterm_get_ctx(), "[KERNEL] Handing over control...\n", 35);
    asm volatile (
    "movw $0x28, %%ax\n\t"
    "ltr %%ax"
    :
    :
    : "ax"
);

//tss_entry.ss0  = 0x10;
uint64_t stack_base_addr = (uint64_t)palloc((STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE, false);
serial_puts("stack base addr: ");
serial_puthex(stack_base_addr);
uint64_t virt_stack_addr = 0x7000000;
map_page(read_cr3(), virt_stack_addr + 0x0000, stack_base_addr + 0x0000, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE);
map_page(read_cr3(), virt_stack_addr + 0x1000, stack_base_addr + 0x1000, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE);
map_page(read_cr3(), virt_stack_addr + 0x2000, stack_base_addr + 0x2000, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE);
map_page(read_cr3(), virt_stack_addr + 0x3000, stack_base_addr + 0x3000, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE);
map_page(read_cr3(), virt_stack_addr + 0x4000, stack_base_addr + 0x4000, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE);

//asm volatile ("mov %%rsp, %0" : "=r"(kernel_stack_top));
stack_top = virt_stack_addr + STACK_SIZE - 8;
//tss_entry.rsp0 = stack_base_addr + STACK_SIZE;  // Set the kernel stack pointer
//tss_entry.io_bitmap_offset = sizeof(tss_entry);  // No I/O permission bitmap

//void* phys_page = palloc(1, false); // allocate one page
char *fd = fat_read(fn, 0);
entry_t elf = load_elf(fd, false);
serial_puts("elf size: ");
serial_puthex(elf_size);
elf_size += 0x2000; // fix me
// Map the page to userspace address, readable + executable + user access
void* phys_page = palloc((elf_size + PAGE_SIZE - 1) / PAGE_SIZE, false); // alloc physical
//void* temp_kernel_mapping = (void*)0x3333906969000000; // pick an unused virtual address in kernel space

// Temporarily map it so kernel can write into it
/*map_page(read_cr3(), (uint64_t)temp_kernel_mapping, (uint64_t)phys_page,
         PAGE_PRESENT | PAGE_WRITABLE);
*/
// start virtual address of user code
user_code_vaddr = find_address(elf_size);
serial_puts("user_code_vaddr: ");
serial_puthex(user_code_vaddr);
// Then map it into user space for execution
for (uint64_t offset = 0; offset < elf_size; offset += PAGE_SIZE) {
    map_page(read_cr3(),
             user_code_vaddr + offset,
             (uint64_t)phys_page + offset,
             PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE | PAGE_EXECUTE);
}

// Now it's safe to memcpy
memcpy((void*)user_code_vaddr, (void*)elf, elf_size);

//memcpy((void*)phys_page, loop, sizeof(loop));
flanterm_write(flanterm_get_ctx(), "[KERNEL] Welcome to userland!\n", 30);
flanterm_write(flanterm_get_ctx(), "\033[0m", 5);

jump_usermode();
}

void demo_userland() {
    char *fn = "usercode";
    enter_userspace(fn);
}

#define PAGE_PAGES(nbytes)  (((nbytes) + PAGE_SIZE - 1) / PAGE_SIZE)

uint64_t find_address(uint64_t elf_size) {
    uint64_t pages_needed = PAGE_PAGES(elf_size);
    uint64_t free_run = 0;
    uint64_t run_start = 0;

    // Start at some safe VA
    uint64_t va = 0x00400000;

    // Arbitrary upper bound
    uint64_t va_end = 0xffffffff80000000;

    for (uint64_t va = 0x00400000; va < va_end; va += MB) {
            if (!is_mapped(va)) {
            // this page is free
            if (free_run == 0)
                run_start = va;
            if (++free_run >= pages_needed)
                return run_start;
        } else {
            // hit an in-use page: reset run
            free_run = 0;
        }
        va += PAGE_SIZE;
    }

    return 0; // no region found
}