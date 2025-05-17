#include <limine.h>
#include "iodebug.h"

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