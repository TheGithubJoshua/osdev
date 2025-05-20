#include <stdint.h>
#include <limine.h>
#include "pmm.h"
#include "../memory.h"
#include "../iodebug.h"
#include "../bitmap.h"

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

uint64_t bitmap_pages;
uint64_t bitmap_size;
uint8_t *bitmap;
static uint64_t free_pages;
static uint64_t page_cache[PAGE_CACHE_SIZE];
static size_t cache_size;
static size_t cache_index;

static inline bool is_aligned(void *addr, size_t align)
{
    return ((uintptr_t)addr % align) == 0;
}

void pmm_init(void) {

    uint64_t high = 0;
    free_pages = 0;

    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap_request.response->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE)
        {
            uint64_t top = e->base + e->length;
            if (top > high)
                high = top;
            free_pages += e->length / PAGE_SIZE;
            //serial_puts("Usable memory region: 0x%.16llx -> 0x%.16llx", e->base, e->base + e->length);
        }
    }

    bitmap_pages = high / PAGE_SIZE;
    bitmap_size = ALIGN_UP(bitmap_pages / 8, PAGE_SIZE);

    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap_request.response->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_size)
        {
            bitmap = (uint8_t *)(e->base + get_phys_offset());
            memset(bitmap, 0xFF, bitmap_size);
            e->base += bitmap_size;
            e->length -= bitmap_size;
            free_pages -= bitmap_size / PAGE_SIZE;
            break;
        }
    }

    cache_size = PAGE_CACHE_SIZE;
    cache_index = 0;
    memset(page_cache, 0, sizeof(page_cache));

    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap_request.response->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE)
        {
            for (uint64_t j = e->base; j < e->base + e->length; j += PAGE_SIZE)
            {
                if ((j / PAGE_SIZE) < bitmap_pages)
                {
                    bitmap_clear(bitmap, j / PAGE_SIZE);
                }
            }
        }
    }
}

void *palloc(size_t pages, bool higher_half) {
    if (pages == 0 || pages > free_pages)
        return NULL;

    if (pages == 1 && cache_index > 0)
    {
        void *addr = (void *)(page_cache[--cache_index] * PAGE_SIZE);
        bitmap_set(bitmap, (uint64_t)addr / PAGE_SIZE);
        free_pages--;
        memset(HIGHER_HALF(addr), 0, pages * PAGE_SIZE);
        return higher_half ? (void *)((uint64_t)addr + get_phys_offset()) : addr;
    }

    uint64_t word_count = (bitmap_pages + BITMAP_WORD_SIZE - 1) / BITMAP_WORD_SIZE;
    uint64_t *bitmap_words = (uint64_t *)bitmap;

    for (uint64_t i = 0; i < word_count; i++)
    {
        if (bitmap_words[i] != UINT64_MAX)
        {
            uint64_t start_bit = i * BITMAP_WORD_SIZE;
            uint64_t consecutive = 0;

            for (uint64_t j = 0; j < BITMAP_WORD_SIZE && start_bit + j < bitmap_pages; j++)
            {
                if (!bitmap_get(bitmap, start_bit + j))
                {
                    if (++consecutive == pages)
                    {
                        for (uint64_t k = 0; k < pages; k++)
                        {
                            bitmap_set(bitmap, start_bit + j - pages + 1 + k);
                        }
                        free_pages -= pages;

                        void *addr = (void *)((start_bit + j - pages + 1) * PAGE_SIZE);
                        memset(HIGHER_HALF(addr), 0, pages * PAGE_SIZE);
                        return higher_half ? (void *)((uint64_t)addr + get_phys_offset()) : addr;
                    }
                }
            }
        }
    }
}

void pfree(void *ptr, size_t pages) {
    if (!ptr || !is_aligned(ptr, MIN_ALIGN))
        return;

    uint64_t start = ((uint64_t)ptr - (get_phys_offset() * ((uint64_t)ptr >= get_phys_offset()))) / PAGE_SIZE;

    if (start + pages > bitmap_pages)
    {
        return;
    }

    for (size_t i = 0; i < pages; i++)
    {
        if (bitmap_get(bitmap, start + i))
        {
            bitmap_clear(bitmap, start + i);
            free_pages++;

            if (pages == 1 && cache_index < cache_size)
            {
                page_cache[cache_index++] = start;
            }
        }
    }

}