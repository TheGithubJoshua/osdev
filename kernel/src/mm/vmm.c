#include "vmm.h"
#include "../iodebug.h"
#include "pmm.h"
#include "../memory.h"

vm_object* vm_list = NULL;

inline uint64_t convert_amd64_vm_flags(size_t flags) {
    uint64_t value = 0;
    if (flags & VM_FLAG_WRITE)
        value |= PAGE_WRITABLE;
    if (flags & VM_FLAG_USER)
        value |= PAGE_USER;
    if ((flags & VM_FLAG_EXEC) == 0)
        value |= PAGE_NO_EXECUTE;
    return value;
};

void vm_insert(vm_object* obj)
{
    if (vm_list == NULL || obj->base < vm_list->base) {
        obj->next = vm_list;
        vm_list = obj;
        return;
    }

    vm_object* cur = vm_list;
    while (cur->next && cur->next->base < obj->base) {
        cur = cur->next;
    }

    obj->next = cur->next;
    cur->next = obj;
}

uintptr_t vm_find_free(size_t size, uintptr_t range_start, uintptr_t range_end)
{
    // if list empty → everything is free
    if (vm_list == NULL) {
        uintptr_t aligned = range_start;
        return (aligned + size <= range_end) ? aligned : 0;
    }

    // check before first object
    vm_object* first = vm_list;
    if (range_start + size <= first->base) {
        return range_start;
    }

    // check gaps between objects
    vm_object* cur = vm_list;
    while (cur->next) {
        uintptr_t gap_start = cur->base + cur->length;
        uintptr_t gap_end   = cur->next->base;

        if (gap_end >= gap_start + size)
            return gap_start;

        cur = cur->next;
    }

    // check after last
    uintptr_t last_end = cur->base + cur->length;
    if (last_end + size <= range_end) {
        return last_end;
    }

    return 0; // no space
}


void* vmm_alloc(size_t length, size_t flags, void* arg) {
    length = PAGE_CEIL(length);

    uintptr_t base = vm_find_free(length, get_usable_mem_base(), get_usable_mem_max());
    if (base == 0)
        return NULL;

    vm_object* obj = palloc((sizeof(vm_object) + PAGE_SIZE - 1) / PAGE_SIZE, true);

    obj->base = base;
    obj->length = length;
    obj->next = NULL;

    vm_insert(obj);

    // allocate some physical memory
    void* phys = palloc((length + PAGE_SIZE - 1) / PAGE_SIZE, false);
    map_len(read_cr3(), obj->base, (uint64_t)phys, convert_amd64_vm_flags(flags), length);

    return (void*)obj->base;
}

void vm_free(void* addr) {
    vm_object* prev = NULL;
    vm_object* current = vm_list;  // head

    while (current) {
        if (current->base == (uintptr_t)addr) {
            // Remove from list
            if (prev)
                prev->next = current->next;
            else
                vm_list = current->next;  // removing head

            current->next = NULL;

            // Free the vm_object struct itself
            pfree((void*)get_phys((uint64_t)addr), current->length);
            pfree(current, (sizeof(vm_object) + PAGE_SIZE - 1) / PAGE_SIZE);
            
            return;  // done
        }

        prev = current;
        current = current->next;
    }

    // base not found
    return;
}
