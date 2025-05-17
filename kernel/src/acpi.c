#include <limine.h>
#include <stddef.h>
#include "hhdm.h"
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

typedef struct rsdp_t {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
} rsdp_t;

typedef struct {
    rsdp_t base;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} rsdp_ext_t;

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

typedef struct ACPISDTHeader {
  char Signature[4];
  uint32_t Length;
  uint8_t Revision;
  uint8_t Checksum;
  char OEMID[6];
  char OEMTableID[8];
  uint32_t OEMRevision;
  uint32_t CreatorID;
  uint32_t CreatorRevision;
} __attribute__((packed)) sdt_header_t;

typedef struct xsdt_t {
    sdt_header_t sdtHeader; //signature "XSDT"
    uint64_t sdtAddresses[];
} __attribute__((packed)) xsdt_t;

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

void *findFACP(void *RootSDT) {
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

    for (int i = 0; i < entries; i++)
    {
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
            return (void *) h;
        }
    }

    // No FACP found
    serial_puts("No FADT found :(\n");
    return NULL;
}

