#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "util/fb.h"

#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER 0x4
#define PAGE_WRITE_THROUGH 0x8
#define PAGE_CACHE_DISABLE 0x10
#define PAGE_ACCESSED 0x20
#define PAGE_DIRTY 0x40
#define PAGE_HUGE 0x80
#define PAGE_GLOBAL 0x100
#define PAGE_NO_EXECUTE (1ULL << 63)
#define PAGE_EXECUTE 0ULL // why not

#define PAGE_MASK (PAGE_SIZE - 1)

#define PAGE_WRITE_EXEC (PAGE_PRESENT | PAGE_WRITABLE)

#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

#define PAGE_SHIFT 12
#define MB (1 << 20)

#define HEAP_MAX (32 * 1024 * 1024) // 32 MB

//#define PAGE_SIZE 4096

// Extract indices from virtual address
#define PML4_INDEX(va) (((va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((va) >> 30) & 0x1FF)
#define PD_INDEX(va)   (((va) >> 21) & 0x1FF)
#define PT_INDEX(va)   (((va) >> 12) & 0x1FF)

#define VM_HIGHER_HALF (get_phys_offset())
#define PHYS_TO_VIRT(phys) (void *)((uintptr_t)phys + VM_HIGHER_HALF)
#define VIRT_TO_PHYS(virt) ((uintptr_t)virt - VM_HIGHER_HALF)

#define virt_to_phys(x) ((uint64_t)(x) - get_phys_offset())

/* Align up/down a value */
#define ALIGN_DOWN(value, align)      ((value) & ~((align)-1))
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define MALIGN(value)                 ALIGN_UP((value), M_WORD_SIZE)
#define PAGE_FLOOR(x) ((x) & ~PAGE_MASK)
#define PAGE_CEIL(x)  (((x) + PAGE_MASK) & ~PAGE_MASK)
#define PAGE_ALIGNED(addr)  (((addr) & (PAGE_SIZE - 1ULL)) == 0)
#define PAGE_PAGES(nbytes)  (((nbytes) + PAGE_SIZE - 1) / PAGE_SIZE)

#define ISSET(v, f)  ((v) & (f))

typedef struct {
    uint64_t present : 1;
    uint64_t writable : 1;
    uint64_t user_mode : 1;
    uint64_t write_through : 1;
    uint64_t cache_disable : 1;
    uint64_t accessed : 1;
    uint64_t dirty : 1;
    uint64_t page_size : 1; // For PDPT and PD entries
    uint64_t global : 1;
    uint64_t ignored0 : 3;
    uint64_t available : 9;
    uint64_t address : 52;
} page_entry_t;

typedef struct {
    page_entry_t entries[512];
} __attribute__((aligned(0x1000))) page_directory_t;

// Assume physical memory identity mapped for page tables access.
typedef uint64_t pt_entry_t;

uint64_t get_phys_offset(void);
void *phys_to_virt(void* phys_addr);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
char *strncpy (char *s1, const char *s2, size_t n);
void quickmap(uint64_t virtual_addr, uint64_t physical_addr);
void map_page(uint64_t pml4_phys, uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);
void map_size(uint64_t phys_addr, uint64_t virt_addr, uint64_t size);
uint64_t read_cr3(void);
pt_entry_t *get_pml4_table(uint64_t pml4_phys);
int is_mapped(uint64_t virtual_addr);
size_t strlenn(const char *s);
void map_len(uint64_t pml4_phys, uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags, size_t len);
uint64_t get_phys(uint64_t virtual_addr);
void* sbrk(intptr_t incr);