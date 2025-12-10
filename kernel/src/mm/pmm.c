#include <stdint.h>
#include <limine.h>
#include "pmm.h"
#include "../panic/panic.h"
#include "../memory.h"
#include "../iodebug.h"

#define PAGE_CACHE_SIZE 1024
#define MIN_ALIGN PAGE_SIZE
#define BITMAP_WORD_SIZE (sizeof(uint64_t) * 8)
#ifndef ALIGN_H
#define ALIGN_H

#define DIV_ROUND_UP(x, y) (((uint64_t)(x) + ((uint64_t)(y) - 1)) / (uint64_t)(y))
#define ALIGN_UP(x, y) (DIV_ROUND_UP(x, y) * (uint64_t)(y))
#define ALIGN_DOWN(x, y) (((uint64_t)(x) / (uint64_t)(y)) * (uint64_t)(y))

#define IS_PAGE_ALIGNED(x) (((uintptr_t)(x) & (PAGE_SIZE - 1)) == 0)

#endif // ALIGN_H

#define LIMINE_MEMMAP_USABLE 0
#define HIGHER_HALF(ptr) ((void *)((uint64_t)(ptr) < get_phys_offset() ? (uint64_t)(ptr) + get_phys_offset() : (uint64_t)(ptr)))

// request memory map
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

uint32_t*   bitmap = NULL;
size_t      size = 0;
size_t      freepages = 0;
size_t      totalpages = 0;

static inline bool is_aligned(void *addr, size_t align)
{
    return ((uintptr_t)addr % align) == 0;
}

void setbit(size_t idx) {
    if (!bitmap) panik("no bitmap");
    size_t totalwords = size / sizeof(uint32_t);
    if (idx / 32 >= totalwords)
        panik("index out of bounds");
    bitmap[idx / 32] |= (1 << (idx % 32));
}

void unsetbit(size_t idx) {
    if (!bitmap) panik("no bitmap");
    size_t totalwords = size / sizeof(uint32_t);
    if (idx / 32 >= totalwords)
        panik("index out of bounds");
    bitmap[idx / 32] &= ~(1 << (idx % 32));
}

bool checkbit(size_t idx) {
    if (!bitmap) panik("no bitmap");
    size_t totalwords = size / sizeof(uint32_t);
    if (idx / 32 >= totalwords)
        panik("index out of bounds");
    return bitmap[idx / 32] & (1 << (idx % 32));
}

uint64_t get_usable_mem_base(void) {
    uint64_t base = UINT64_MAX;  // start high so we find the minimum

    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *ent = memmap_request.response->entries[i];

        if (ent->type == LIMINE_MEMMAP_USABLE) {
            if (ent->base < base) {
                base = ent->base;
            }
        }
    }

    return (base == UINT64_MAX) ? 0 : base; // if none found, return 0 or handle error
}

uint64_t get_usable_mem_max(void) {
    uint64_t maxtopaddr = 0;

    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *ent = memmap_request.response->entries[i];

        if (ent->type == LIMINE_MEMMAP_USABLE) {
            uint64_t topaddr = ent->base + ent->length;

            if (topaddr > maxtopaddr) {
                maxtopaddr = topaddr;
            }
        }
    }

    return maxtopaddr;
}

void pmm_init(void) {
    if (memmap_request.response == NULL) panik("null memmap response.");

    uint64_t offset = get_phys_offset();
    uintptr_t maxtopaddr = 0;
    for (size_t i = 0; i < memmap_request.response->entry_count; i++)
        if (memmap_request.response->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            uintptr_t topaddr = memmap_request.response->entries[i]->base +
                                memmap_request.response->entries[i]->length;
            if (topaddr > maxtopaddr) maxtopaddr = topaddr;
        }

    size = (maxtopaddr / PAGE_SIZE / sizeof(uint32_t)) + 1;
    totalpages = (maxtopaddr / PAGE_SIZE) + 1;
    for (size_t i = 0; i < memmap_request.response->entry_count; i++)
        if (memmap_request.response->entries[i]->type == LIMINE_MEMMAP_USABLE &&
            memmap_request.response->entries[i]->length >= size) {
            bitmap = (uint32_t*)(memmap_request.response->entries[i]->base + offset);
            memset(bitmap, 0xffffffff, size * sizeof(uint32_t));
            break;
        }

    if (bitmap == NULL) panik("bitmap doesn't fit into memory.");

    for (size_t i = 0; i < memmap_request.response->entry_count; i++)
        if (memmap_request.response->entries[i]->type == LIMINE_MEMMAP_USABLE)
            for (uint64_t p = 0; p < memmap_request.response->entries[i]->length;
                p += PAGE_SIZE) {
                uintptr_t physaddr = memmap_request.response->entries[i]->base + p;
                uintptr_t startaddr = (uintptr_t)bitmap - offset;
                uintptr_t finaladdr = startaddr + size * sizeof(uint32_t);
                if (physaddr < startaddr || physaddr >= finaladdr) {
                    unsetbit(physaddr / PAGE_SIZE);
                    freepages++;
                }
            }
}

void* palloc(size_t count, bool higher_half) {
    if (count == 0) panik("allocated 0 page frames.");
    if (count > totalpages) panik_no_mem();

    for (size_t i = 0; i + count <= totalpages; i++) {
        bool found = true;
        for (size_t k = 0; k < count; k++) if (checkbit(i + k) != 0) {
            found = false;
            i += k;
            break;
        }

        if (found) {
            for (size_t k = 0; k < count; k++) setbit(i + k);
            freepages -= count;
            return higher_half ? (void *)((uint64_t)(i * PAGE_SIZE) + get_phys_offset()) : (void*)(i * PAGE_SIZE);
        }
    }

    panik_no_mem();
}

void pfree(void* frameptr, size_t count) {
    frameptr = (void*)HH_TO_LH(frameptr);
    uint64_t frameidx = (uintptr_t)frameptr / 4096;
    for (uint64_t i = frameidx; i < frameidx + count; i++) unsetbit(i);
    freepages += count;
}