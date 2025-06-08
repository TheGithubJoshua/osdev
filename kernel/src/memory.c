#include <limine.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "iodebug.h"
#include "mm/pmm.h"
#include "memory.h"

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

//#define PAGE_SIZE 4096

// Extract indices from virtual address
#define PML4_INDEX(va) (((va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((va) >> 30) & 0x1FF)
#define PD_INDEX(va)   (((va) >> 21) & 0x1FF)
#define PT_INDEX(va)   (((va) >> 12) & 0x1FF)

#define virt_to_phys(x) ((uint64_t)(x) - get_phys_offset())

// HHDM (Higher-Half Direct Map) request
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    //.response = NULL  // Will be populated by the bootloader
};

// Get the physical-to-virtual offset
uint64_t get_phys_offset(void) {
    if (!hhdm_request.response) {
        serial_puts("HHDM not supported by bootloader");
        return 0;  // Handle error
    }
    return hhdm_request.response->offset;
}

void *phys_to_virt(void* phys_addr) {
    uint64_t phys_offset = get_phys_offset();
    return (void *)(phys_addr + phys_offset);
}
void *memcpy(void *restrict dest, const void *restrict src, size_t n) {

    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

char *
strncpy (s1, s2, n)
     char *s1;
     const char *s2;
     size_t n;
{
  char c;
  char *s = s1;

  --s1;

  if (n >= 4)
    {
      size_t n4 = n >> 2;

      for (;;)
    {
      c = *s2++;
      *++s1 = c;
      if (c == '\0')
        break;
      c = *s2++;
      *++s1 = c;
      if (c == '\0')
        break;
      c = *s2++;
      *++s1 = c;
      if (c == '\0')
        break;
      c = *s2++;
      *++s1 = c;
      if (c == '\0')
        break;
      if (--n4 == 0)
        goto last_chars;
    }
      n = n - (s1 - s) - 1;
      if (n == 0)
    return s;
      goto zero_fill;
    }

 last_chars:
  n &= 3;
  if (n == 0)
    return s;

  do
    {
      c = *s2++;
      *++s1 = c;
      if (--n == 0)
    return s;
    }
  while (c != '\0');

 zero_fill:
  do
    *++s1 = '\0';
  while (--n > 0);

  return s;
}

// Assume physical memory identity mapped for page tables access.
typedef uint64_t pt_entry_t;

static pt_entry_t *get_pml4_table(uint64_t pml4_phys) {
    return (pt_entry_t *)pml4_phys;
}

// Allocates one 4KB page for a new page table

void map_page(uint64_t pml4_phys, uint64_t virtual_addr,
              uint64_t physical_addr, uint64_t flags)
{

    // Convert CR3->PML4 physical into a kernel-virtual pointer
    pt_entry_t *pml4 = get_pml4_table(pml4_phys);

    // ——— Walk PML4 → PDPT ———
    pt_entry_t *pdpt;
    if (!(pml4[PML4_INDEX(virtual_addr)] & PAGE_PRESENT)) {
        pdpt = palloc(1, true);
        memset(pdpt, 0, 512 * sizeof(pt_entry_t));

        uint64_t pdpt_phys = ((uint64_t)pdpt - get_phys_offset()) & ~0xFFFULL;


        pml4[PML4_INDEX(virtual_addr)] =
            pdpt_phys | PAGE_PRESENT | PAGE_WRITABLE;
    } else {
        uint64_t pdpt_phys = pml4[PML4_INDEX(virtual_addr)] & ~0xFFFULL;

        pdpt = (pt_entry_t *)(pdpt_phys + get_phys_offset());
    }

    // ——— Walk PDPT → PD ———
    pt_entry_t *pd;
    if (!(pdpt[PDPT_INDEX(virtual_addr)] & PAGE_PRESENT)) {
        pd = palloc(1, true);
        memset(pd, 0, 512 * sizeof(pt_entry_t));

        uint64_t pd_phys = ((uint64_t)pd - get_phys_offset()) & ~0xFFFULL;


        pdpt[PDPT_INDEX(virtual_addr)] =
            pd_phys | PAGE_PRESENT | PAGE_WRITABLE;
    } else {
        uint64_t pd_phys = pdpt[PDPT_INDEX(virtual_addr)] & ~0xFFFULL;
        
        pd = (pt_entry_t *)(pd_phys + get_phys_offset());
    }

    // ——— Walk PD → PT ———
    pt_entry_t *pt;
    if (!(pd[PD_INDEX(virtual_addr)] & PAGE_PRESENT)) {
        pt = palloc(1, true);
        memset(pt, 0, 512 * sizeof(pt_entry_t));

        uint64_t pt_phys = ((uint64_t)pt - get_phys_offset()) & ~0xFFFULL;

        pd[PD_INDEX(virtual_addr)] =
            pt_phys | PAGE_PRESENT | PAGE_WRITABLE;
    } else {
        uint64_t pt_phys = pd[PD_INDEX(virtual_addr)] & ~0xFFFULL;
        pt = (pt_entry_t *)(pt_phys + get_phys_offset());
    }

    // ——— Finally map the 4 KiB page ———


    pt[PT_INDEX(virtual_addr)] =
        (physical_addr & ~0xFFFULL) | flags | PAGE_PRESENT;

    // Invalidate TLB entry for the page
    __asm__ volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
}


static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r" (cr3));
    return cr3;
}

void map_nvme_mmio(uint64_t virtual_addr, uint64_t physical_addr) {
    uint64_t cr3 = read_cr3() + get_phys_offset(); // get current PML4 physical address

    // Flags: writable, no cache (cache disable)
    uint64_t flags = PAGE_WRITABLE | PAGE_CACHE_DISABLE;

    map_page(cr3, virtual_addr, physical_addr, flags);

    // Reload CR3 to flush full TLB if you want:
    // write_cr3(cr3);
}

void map_size(uint64_t phys_addr, uint64_t virt_addr, uint64_t size) {
    // Loop over each page in size
    for (uint64_t offset = 0; offset < size; offset += PAGE_SIZE) {
        map_nvme_mmio(phys_addr + offset, virt_addr + offset);
    }
}

