#pragma once
#include <stdint.h>
#include <stddef.h>

#define VM_FLAG_NONE 0
#define VM_FLAG_WRITE (1 << 0)
#define VM_FLAG_EXEC (1 << 1)
#define VM_FLAG_USER (1 << 2)

typedef struct vm_object {
    uintptr_t base;
    size_t length;
    size_t flags;
    struct vm_object* next;
} vm_object;

void* vmm_alloc(size_t length, size_t flags, void* arg);
void vm_free(void* addr);
inline uint64_t convert_amd64_vm_flags(size_t flags);
