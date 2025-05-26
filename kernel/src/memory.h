#include <stddef.h>
#include <stdint.h>

uint64_t get_phys_offset(void);
void *phys_to_virt(void* phys_addr);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
char *strncpy (char *s1, const char *s2, size_t n);
