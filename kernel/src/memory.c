#include <limine.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "iodebug.h"
#include "memory.h"

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
