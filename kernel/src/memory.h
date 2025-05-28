#include <stddef.h>
#include <stdint.h>

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

