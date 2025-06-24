#include <stddef.h>
#include <stdint.h>

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

#define PAGE_WRITE_EXEC (PAGE_PRESENT | PAGE_WRITABLE)

#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

//#define PAGE_SIZE 4096

// Extract indices from virtual address
#define PML4_INDEX(va) (((va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((va) >> 30) & 0x1FF)
#define PD_INDEX(va)   (((va) >> 21) & 0x1FF)
#define PT_INDEX(va)   (((va) >> 12) & 0x1FF)

#define virt_to_phys(x) ((uint64_t)(x) - get_phys_offset())

uint64_t get_phys_offset(void);
void *phys_to_virt(void* phys_addr);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
char *strncpy (char *s1, const char *s2, size_t n);
void map_nvme_mmio(uint64_t virtual_addr, uint64_t physical_addr);
void map_page(uint64_t pml4_phys, uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);
void map_size(uint64_t phys_addr, uint64_t virt_addr, uint64_t size);
uint64_t read_cr3(void);
