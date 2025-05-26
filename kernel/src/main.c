#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <lai/helpers/pm.h>
#include <lai/helpers/sci.h>
#include "flanterm/flanterm.h"
#include "iodebug.h"
#include "interrupts/interrupts.h"
#include "acpi/acpi.h"
#include "lai/include/lai/core.h"
#include "mm/pmm.h"
#include "timer.h"
#include "util/fb.h"
#include "memory.h"
#include "thread/thread.h"
#include "cpu/msr.h"
//#include "liballoc/liballoc.h"
#include "flanterm/backends/fb.h"
#include "interrupts/apic.h"
#include "font.c"

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

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

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

// im putting io here bc im lazy
// not anymore

static inline bool are_interrupts_enabled()
{
    unsigned long flags;
    asm volatile ( "pushf\n\t"
                   "pop %0"
                   : "=g"(flags) );
    return flags & (1 << 9);
}

bool is_long_mode() {
    uint32_t a, d;
    asm volatile (
        "cpuid"
        : "=a"(a), "=d"(d)
        : "a"(0x80000001)
        : "ebx", "ecx"
    );
    return (d & (1 << 29));  // LM bit in CPUID.80000001h:EDX[29]
}

void int_to_str(int value, char *str) {
    char temp[12]; // enough for 32-bit int
    int i = 0;
    int is_negative = 0;

    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    if (value < 0) {
        is_negative = 1;
        value = -value;
    }

    while (value > 0) {
        temp[i++] = (value % 10) + '0';
        value /= 10;
    }

    if (is_negative) {
        temp[i++] = '-';
    }

    // Reverse the string
    int j = 0;
    while (i > 0) {
        str[j++] = temp[--i];
    }

    str[j] = '\0';
}

#define GDT_KERNEL_CODE_SELECTOR 0x08

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.

void test_palloc() {
    struct flanterm_context *ft_ctx = flanterm_get_ctx();
    void *ptr = palloc(1, true);  // Allocate 1 page, map to higher half

    if (!ptr) {
        serial_puts("palloc() failed: returned NULL\n");
        return;
    }

    serial_puts("palloc() returned address: ");
    serial_puthex((uintptr_t)ptr);
    serial_puts("\n");

    // Test writing to the allocated memory
    memset(ptr, 0xAA, PAGE_SIZE);

    // Test reading back
    if (((uint8_t *)ptr)[0] != 0xAA) {
        flanterm_write(ft_ctx, "\033[31mpalloc memory test failed: value mismatch!\033[0m\n", 47);
    } else {
        flanterm_write(ft_ctx, "\033[32mpalloc memory test passed!\033[0m\n", 36);
        pfree(ptr, 1);
    }
}

void log_acpi_subtree(lai_nsnode_t* parent, int depth) {
    struct lai_ns_child_iterator iter = LAI_NS_CHILD_ITERATOR_INITIALIZER(parent);
    lai_nsnode_t* node;

    while ((node = lai_ns_child_iterate(&iter))) {
        for (int i = 0; i < depth; i++) serial_puts("  ");
        serial_puts("Node: ");
        serial_puts(node->name);
        serial_puts("\n");
        log_acpi_subtree(node, depth + 1);
    }
}

void log_acpi_namespace(void) {
    lai_nsnode_t* root = lai_resolve_path(NULL, "\\");
    struct lai_ns_child_iterator iter = LAI_NS_CHILD_ITERATOR_INITIALIZER(root);
    lai_nsnode_t* node;

    while ((node = lai_ns_child_iterate(&iter))) {
        serial_puts("ACPI Node: ");
        serial_puts(node->name);
        serial_puts("\n");

        // Recursively log children
        log_acpi_subtree(node, 1);
    }
}

void test_pci_readb() {
    uint16_t seg = 0;      // Usually 0 if you have only one PCI segment
    uint8_t bus = 0;       // Bus 0 usually exists
    uint8_t slot = 0;      // Device 0 on bus 0
    uint8_t fun = 0;       // Function 0
    uint16_t offset = 0x00; // Vendor ID offset

    uint8_t val = laihost_pci_readb(seg, bus, slot, fun, offset);
    // Vendor ID is 2 bytes, so reading offset 0x00 gives the low byte
    serial_puts("PCI Vendor ID (low byte): ");
    serial_puthex(val);
}

void ioapic_unmask_all(uintptr_t ioapic_base) {
    // Read max IRQs supported from IOAPIC version register
    uint32_t version = read_ioapic_register(ioapic_base, 0x01);
    uint8_t max_irq = (version >> 16) & 0xFF;

    for (uint8_t irq = 0; irq <= max_irq; irq++) {
        // Read existing redirection table entry
        uint32_t lo = read_ioapic_register(ioapic_base, 0x10 + irq * 2);
        uint32_t hi = read_ioapic_register(ioapic_base, 0x10 + irq * 2 + 1);

        // Clear mask bit (bit 16)
        lo &= ~(1 << 16);

        // Optionally set a fixed interrupt vector (e.g., 0x20 + irq)
        lo = (lo & ~0xFF) | (0x20 + irq);  // Just an example

        // Rewrite redirection entry
        write_ioapic_register(ioapic_base, 0x10 + irq * 2, lo);
        write_ioapic_register(ioapic_base, 0x10 + irq * 2 + 1, hi);
    }
}

void ioapic_remap_irq(uintptr_t ioapic_base, uint8_t irq, uint8_t vector, uint8_t apic_id) {
    uint32_t entry_low = 0;
    uint32_t entry_high = 0;

    // Set interrupt vector
    entry_low = vector;

    // Delivery mode = 0 (fixed)
    // Trigger mode = 0 (edge) for legacy IRQs
    // Polarity = 0 (active high)
    // Mask = 0 (enabled)

    // Set destination APIC ID
    entry_high = ((uint32_t)apic_id) << 24;

    write_ioapic_register(ioapic_base, 0x10 + irq * 2, entry_low);
    write_ioapic_register(ioapic_base, 0x10 + irq * 2 + 1, entry_high);
}

void ioapic_remap_all(uintptr_t ioapic_base, uint8_t apic_id) {
    for (uint8_t irq = 0; irq < 16; irq++) {
        ioapic_remap_irq(ioapic_base, irq, 0x20 + irq, apic_id);
    }
}

void kmain(void) {

    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    /*if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }
*/

/* get shit setup */
    idt_init();
    //irq_unmask_all();

    irq_remap();
    //timer_handler();
    enable_apic();
    timer_phase(100); // 100 Hz
    apic_start_timer();

    rsdp_t *rsdp = get_acpi_table();
    struct xsdt_t *xsdt = get_xsdt_table();
    fadt_t *fadt = get_fadt(get_xsdt_table());

    init_flanterm();
    struct flanterm_context *ft_ctx = flanterm_get_ctx();
    pmm_init();
    //initialise_multitasking();
    //timer_wait(5);
    lai_create_namespace();
    lai_set_acpi_revision(rsdp->revision);
    //lai_enable_acpi(0);
    outb(fadt->SMI_CommandPort,fadt->AcpiEnable);
    uint16_t val = inw(fadt->PM1aControlBlock);
    bool acpi_enabled = val & 1;
    if (acpi_enabled)
        serial_puts("acpi enabled! \n");
    
    //beep();
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
//    for (size_t i = 0; i < 100; i++) {
//        volatile uint32_t *fb_ptr = framebuffer->address;
//        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
//    }

// demo fault
asm("int $0");

serial_puts("hello \n");
flanterm_write(ft_ctx, "Welcome!\n", 10);

//log_acpi_namespace();
test_palloc();
//test_pci_readb();
uint32_t ioapic_ver = read_ioapic_register(IOAPIC_VIRT_ADDR, 0x01);
uint8_t version = ioapic_ver & 0xFF;
uint8_t max_redir_entries = ((ioapic_ver >> 16) & 0xFF) + 1;

serial_puts("IOAPIC Version: ");
serial_puthex(version);
serial_puts(", Redir entries: ");
serial_puthex(max_redir_entries);

uintptr_t ioapic_base = get_ioapic_addr() + get_phys_offset(); // or mapped address
uint8_t apic_id = 0; // CPU's LAPIC ID
ioapic_remap_all(ioapic_base, apic_id);
ioapic_unmask_all(ioapic_base);
initialise_multitasking();

// We're done, just hang...
    hcf();
}


