#include <limine.h>
#include <stddef.h>
#include <stdint.h>
#include <lai/core.h>
#include <lai/helpers/sci.h>
#include "memory.h"
#include "acpi.h"
#include "iodebug.h"

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
//static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

/*static int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}*/

typedef struct {
    rsdp_t base;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} rsdp_ext_t;

typedef struct xsdt_t {
    sdt_header_t sdtHeader; //signature "XSDT"
    uint64_t sdtAddresses[];
} __attribute__((packed)) xsdt_t;

int rsdp_validate(rsdp_t *rsdp) {
    // Expected signature "RSD PTR " (8 bytes)
    const char *expected = "RSD PTR ";

    // Check each byte of the signature
for (int i = 0; i < 8; i++) {
        if (rsdp->signature[i] != expected[i]) {
            serial_puts("Invalid RSDP signature");
            return 0;  // Validation failed
        }
    }

    // Optionally: Verify checksum here
    return 1;  // Validation passed
}

int strncmp(const char *s1, const char *s2, register size_t n)
{
  register unsigned char u1, u2;

  while (n-- > 0)
    {
      u1 = (unsigned char) *s1++;
      u2 = (unsigned char) *s2++;
      if (u1 != u2)
    return u1 - u2;
      if (u1 == '\0')
    return 0;
    }
  return 0;
}

rsdp_t *get_acpi_table(void) {

    if (!rsdp_request.response || !rsdp_request.response->address) {
        serial_puts("ACPI RSDP not found");
    }
    
    rsdp_t *rsdp = (rsdp_t *)rsdp_request.response->address;
    rsdp_ext_t *rsdp_ext = (rsdp_ext_t *)rsdp;
    
    serial_puthex((uintptr_t)rsdp);
    //serial_puthex(get_phys_offset());
    
    // Basic validation
    if (rsdp_validate(rsdp) == 0) {
        serial_puts("Invalid RSDP signature");
        return 0;
    }

    serial_puts("Found ACPI RSDP at ");
    serial_puthex((uintptr_t)rsdp);
    serial_puts("\n");
    serial_puthex(rsdp_ext->xsdt_address);
    serial_puts("\n");
    serial_puts("RSDP revision: ");
    serial_puthex(rsdp->revision); // Should be ≥ 2 for XSDT

    return rsdp;
}

// now for the xsdt shit.

xsdt_t *get_xsdt_table(void) {
	//xsdt_t xsdt;
	//sdt_header_t *sdt_header (sdt_header_t *)rsdp_ext_t.xsdt_address
	//serial_puthex(xsdt.sdtAddresses[1]);
    rsdp_t *rsdp = (rsdp_t *)rsdp_request.response->address;
    rsdp_ext_t *rsdp_ext = (rsdp_ext_t *)rsdp;
    xsdt_t *xsdt = (xsdt_t *)((uintptr_t)rsdp_ext->xsdt_address + get_phys_offset());
    //serial_puthex((uint64_t)xsdt);

    //sdt_header_t* header = (sdt_header_t*)(xsdt->sdtAddresses[1]);
    //serial_puts("\n");
    //size_t number_of_items = (xsdt->sdtHeader.Length - sizeof(sdt_header_t)) / 8; //GPF
    //serial_puthex(number_of_items);
    //serial_puthex(xsdt->sdtAddresses[1]);

    return xsdt;
}

fadt_t *get_fadt(void *RootSDT) {
    xsdt_t *xsdt = (xsdt_t *) RootSDT;
    int entries = (xsdt->sdtHeader.Length - sizeof(xsdt->sdtHeader)) / 8;
    
    serial_puts("\nTesting strncmp!\n");
    char test1[5] = "test";
    char test2[5] = "test";
    if (strncmp(test1, test2, 4) == 0) {
        serial_puts("strncmp test passed! contuning...\n");
    } else {
        serial_puts("strncmp test failed!\n");
    }

    for (int i = 0; i < entries; i++) {
        struct ACPISDTHeader *h = (struct ACPISDTHeader *)((uintptr_t)xsdt->sdtAddresses[i] + get_phys_offset());
        //serial_puts("\n");
        //serial_puthex((uint64_t)(uintptr_t)&h->Signature);
        //serial_puts("\n");

        //serial_puts("sdtAddresses[1]: \n");
        //serial_puthex(xsdt->sdtAddresses[i] + get_phys_offset());
        //serial_puts("\n");

        serial_puts("\nEntry ");
        serial_puthex(i);
        serial_puts(" Signature: ");
        for (int j = 0; j < 4; j++) serial_putc(h->Signature[j]);
        serial_puts("\nLength: ");
        serial_puthex(h->Length);

        if (!strncmp(h->Signature, "FACP", 4)) {
            serial_puts("\n FADT Found! Loc: ");
            serial_puthex(xsdt->sdtAddresses[i]);
            serial_puts("\n");
            fadt_t *fadt = (struct fadt_t *)((uintptr_t)xsdt->sdtAddresses[i] + get_phys_offset());
            serial_puthex(fadt->SMI_CommandPort);
            serial_puts("\n");
            serial_puthex(fadt->PM1aControlBlock);
            return fadt;
        }
    }

    // No FACP found
    serial_puts("No FADT found :(\n");
    return NULL;
}

/*fadt_t *parse_fadt(void) {
    fadt_t *fadt = (fadt_t *)get_fadt(get_xsdt_table());
    if (fadt == NULL) {
        serial_puts("FADT not found");
        return 0;
    }

    // Print some fields from the FADT
    serial_puts("FADT Signature: ");
    for (int i = 0; i < 4; i++) serial_putc(fadt->h.Signature[i]);
    serial_puts("\nLength: ");
    serial_puthex(fadt->h.Length);
    serial_puts("\nFirmware Control: ");
    serial_puthex(fadt->FirmwareCtrl);
    serial_puts("\nDSDT: ");
    serial_puthex(fadt->Dsdt);

    return fadt;
}*/

void *get_madt(void *RootSDT) {
    xsdt_t *xsdt = (xsdt_t *) RootSDT;
    int entries = (xsdt->sdtHeader.Length - sizeof(xsdt->sdtHeader)) / 8;

        for (int i = 0; i < entries; i++) {
        struct ACPISDTHeader *h = (struct ACPISDTHeader *)((uintptr_t)xsdt->sdtAddresses[i] + get_phys_offset());

        if (!strncmp(h->Signature, "APIC", 4)) {
            serial_puts("\n MADT Found! Loc: ");
            serial_puthex(xsdt->sdtAddresses[i]);
            serial_puts("\n");
            madt_t *madt = (struct madt_t *)((uintptr_t)xsdt->sdtAddresses[i] + get_phys_offset());
            return madt;
        }
    }
    // no madt :(
    serial_puts("no madt found :(");
}

static uint32_t addr;

void parse_madt(madt_t* madt) {
    uint8_t* end = (uint8_t*)madt + madt->header.Length;
    uint8_t* ptr = madt->entries;

    while (ptr < end) {
        uint8_t type = ptr[0];
        uint8_t len = ptr[1];

        switch (type) {
            case 0: {
                // Processor Local APIC
                uint8_t acpi_id = ptr[2];
                uint8_t apic_id = ptr[3];
                uint32_t flags = *(uint32_t*)(ptr + 4);
                break;
            }
            case 1: {
                // IO APIC
                uint8_t id = ptr[2];
                addr = *(uint32_t*)(ptr + 4);
                uint32_t gsi_base = *(uint32_t*)(ptr + 8);
                serial_puts("IOAPIC found at ");
                serial_puthex(addr);
                serial_puts("!\n");
                break;
            }
            // more here
        }

        ptr += len;
    }
}

uint32_t get_ioapic_addr() { parse_madt(get_madt(get_xsdt_table())); return addr; }
// more here

void *get_table(const char* signature, size_t index) {
    // Currently only supports 'DSDT' and 'FACP' and ignores index

    if (strncmp(signature, "DSDT", 4) == 0) {
        fadt_t *fadt = (fadt_t *)get_fadt(get_xsdt_table());
        void *dsdt_virt = (void *)((uintptr_t)fadt->Dsdt + get_phys_offset());
        if (fadt == NULL) {
            serial_puts("FADT not found");
            return NULL;
        }
        return dsdt_virt;
    } else if (strncmp(signature, "FACP", 4) == 0) {
        fadt_t *fadt = (fadt_t *)get_fadt(get_xsdt_table());
        if (fadt == NULL) {
            serial_puts("FADT not found");
            return NULL;
        }
        return (void *)fadt;
    } else if (strncmp(signature, "RSD PTR ", 8) == 0) {
        return get_acpi_table();
    }

    // If signature is unsupported
    return NULL;
}