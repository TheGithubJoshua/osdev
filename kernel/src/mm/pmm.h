#ifndef PMM_H
#define PMM_H

#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 0x1000

#define HH_TO_LH(addr) \
    (((uint64_t)(addr) >= get_phys_offset()) ? \
        ((uint64_t)(addr) - get_phys_offset()) : \
        (uint64_t)(addr))

void pmm_init();
void* palloc(size_t count, bool higher_half);
void pfree(void* frameptr, size_t count);
uint64_t get_usable_mem_base();
uint64_t get_usable_mem_max();
#endif // PMM_H