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
#include "panic/panic.h"
#include "buffer/buffer.h"
#include "timer.h"
#include "util/fb.h"
#include "memory.h"
#include "elf/elf.h"
#include "thread/thread.h"
#include "userspace/enter.h"
#include "drivers/fat/fat.h"
#include "drivers/nvme/nvme.h"
#include "cpu/msr.h"
//#include "liballoc/liballoc.h"
#include "flanterm/backends/fb.h"
#include "interrupts/apic.h"
#include "font.c"

uintptr_t kernel_stack_top;
uint8_t kernel_stack[KERNEL_STACK_SIZE] __attribute__((aligned(16)));

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

typedef struct gtdr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

uint64_t gdt_entries[7];
uint64_t num_gdt_entries = 7;

gdtr_t gdtr = {
    .limit = 0,  // initialize later
    .base = 0
};

struct tss_entry_struct tss __attribute__((aligned(16)));

// Function to create a TSS entry for the GDT
void create_tss_entry(void* tss_base, uint8_t index) {
    uint64_t base = (uint64_t)tss_base;
    uint64_t limit = sizeof(struct tss_entry_struct) - 1;

    uint64_t low = 0;
    low |= (limit & 0xFFFF);
    low |= (base & 0xFFFFFF) << 16;
    low |= (uint64_t)0x89 << 40; // Type=9 (TSS), P=1, S=0
    low |= ((limit >> 16) & 0xF) << 48;
    low |= ((base >> 24) & 0xFF) << 56;

    uint64_t high = (base >> 32);

    gdt_entries[index] = low;
    gdt_entries[index + 1] = high;
}

void load_gdt() {
    asm volatile ("cli");
    gdt_entries[0] = 0;

    uint64_t kernel_code = 0;
    kernel_code |= 0b1011 << 8; //type of selector
    kernel_code |= 1 << 12; //not a system descriptor
    kernel_code |= 0 << 13; //DPL field = 0
    kernel_code |= 1 << 15; //present
    kernel_code |= 1 << 21; //long-mode segment
    gdt_entries[1] = kernel_code << 32;

    uint64_t kernel_data = 0;
    kernel_data |= 0b0011 << 8; //type of selector
    kernel_data |= 1 << 12; //not a system descriptor
    kernel_data |= 0 << 13; //DPL field = 0
    kernel_data |= 1 << 15; //present
    kernel_data |= 1 << 21; //long-mode segment
    gdt_entries[2] = kernel_data << 32;

    uint64_t user_code = 0;
    user_code |= (0b1011ULL << 8);  // Executable, readable
    user_code |= (1ULL << 12);      // S = 1 (code/data descriptor)
    user_code |= (3ULL << 13);      // DPL = 3 (ring 3)
    user_code |= (1ULL << 15);      // Present
    user_code |= (1ULL << 21);      // L = 1 (64-bit code)
    user_code |= (1ULL << 23);      // Granularity = 1 (4KiB)
    gdt_entries[3] = user_code << 32;

    uint64_t user_data = 0;
    user_data |= 0b0011ULL << 8;   // type: data segment, read/write
    user_data |= 1ULL << 12;       // S = 1: code/data
    user_data |= 3ULL << 13;       // DPL = 3
    user_data |= 1ULL << 15;       // Present
    user_data |= 1ULL << 21;       // Long mode (harmless for data)
    user_data |= 1ULL << 23;       // Granularity
    gdt_entries[4] = user_data << 32;

    // zero tss
    //memset(&tss, 0, sizeof(tss_entry_t)); // why triple fault WHY

    void* tss_base = &tss;

    // setup kernel stack for returning to ring 0 for interrupts etc.
    tss.rsp0 = (uintptr_t)(kernel_stack + KERNEL_STACK_SIZE);

    create_tss_entry(tss_base, 5); // Set the TSS entry in the GDT (at index 5)

    // Initialize gdtr fields before loading
    gdtr.limit = (uint16_t)(num_gdt_entries * sizeof(uint64_t) - 1);
    gdtr.base = (uint64_t)gdt_entries;

    asm volatile ("lgdt %0" : : "m"(gdtr));
    asm volatile("\
           mov $0x10, %ax \n\
           mov %ax, %ds \n\
           mov %ax, %es \n\
           mov %ax, %fs \n\
           mov %ax, %gs \n\
           mov %ax, %ss \n\
           \n\
           pop %rdi \n\
           push $0x8 \n\
           push %rdi \n\
           lretq \n\
       ");
    //asm volatile ("movw (5 * 8) | 0, %%ax; ltr %%ax" : : : "ax");
    asm volatile ("sti");
    serial_puts("fucking hello??");
}

void print_gdt_entry(uint64_t entry, int index) {
    uint16_t limit_low    = entry & 0xFFFF;
    uint16_t base_low     = (entry >> 16) & 0xFFFF;
    uint8_t  base_mid     = (entry >> 32) & 0xFF;
    uint8_t  access       = (entry >> 40) & 0xFF;
    uint8_t  limit_high   = (entry >> 48) & 0x0F;
    uint8_t  flags        = (entry >> 52) & 0x0F;
    uint8_t  base_high    = (entry >> 56) & 0xFF;

    uint32_t base = (base_low) | (base_mid << 16) | (base_high << 24);
    uint32_t limit = limit_low | (limit_high << 16);

    // Print header
    serial_puts("GDT Entry ");
    serial_puthex(index);
    serial_puts(":\n");

    // Base
    serial_puts("  Base: 0x");
    serial_puthex(base);
    serial_puts("\n");

    // Limit
    serial_puts("  Limit: 0x");
    serial_puthex(limit);
    serial_puts("\n");

    // Access
    serial_puts("  Access: 0x");
    serial_puthex(access);
    serial_puts("\n");

    serial_puts("    Present: ");
    serial_puthex((access >> 7) & 1);
    serial_puts("\n");

    serial_puts("    DPL: ");
    serial_puthex((access >> 5) & 3);
    serial_puts("\n");

    serial_puts("    S (Descriptor type): ");
    serial_puthex((access >> 4) & 1);
    serial_puts("\n");

    serial_puts("    Executable: ");
    serial_puthex((access >> 3) & 1);
    serial_puts("\n");

    serial_puts("    Direction/Conforming: ");
    serial_puthex((access >> 2) & 1);
    serial_puts("\n");

    serial_puts("    Readable/Writable: ");
    serial_puthex((access >> 1) & 1);
    serial_puts("\n");

    serial_puts("    Accessed: ");
    serial_puthex(access & 1);
    serial_puts("\n");

    // Flags
    serial_puts("  Flags: 0x");
    serial_puthex(flags);
    serial_puts("\n");

    serial_puts("    Granularity (4KiB blocks): ");
    serial_puthex((flags >> 3) & 1);
    serial_puts("\n");

    serial_puts("    64-bit code segment (L bit): ");
    serial_puthex((flags >> 1) & 1);
    serial_puts("\n");

    serial_puts("    Default operation size (D/B bit): ");
    serial_puthex((flags >> 2) & 1);
    serial_puts("\n");

    serial_puts("\n");
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

/*asm volatile ("sgdt %0" : "=m"(gdtr));

#define GDT_ENTRIES 9
uint64_t new_gdt[GDT_ENTRIES] = {0};  // Each entry is 8 bytes (64 bits)

// Copy old GDT (assumes 3 entries, each 8 bytes)
memcpy(new_gdt, (void *)gdtr.base, gdtr.limit + 1);

// User Code Segment (Ring 3, 64-bit)
new_gdt[3] = 0x00AF9A000000FFFFULL;
// Base = 0, Limit = 0xFFFFF, Flags = 64-bit, Present, Ring 3, Execute/Read

// User Data Segment (Ring 3)
new_gdt[4] = 0x00AF92000000FFFFULL;
// Same as above, but for Data (Read/Write)

// add tss segment to gdt
tss_entry_t tss_entry;

uint64_t base = (uint64_t)&tss_entry;
uint16_t limit = sizeof(tss_entry_t) - 1; // usually 0x67
*/
/*new_gdt[5] = ((uint64_t)(limit & 0xFFFF)) |
             ((uint64_t)(base & 0xFFFF) << 16) |
             ((uint64_t)((base >> 16) & 0xFF) << 32) |
             ((uint64_t)0x89 << 40) |
             ((uint64_t)((limit >> 16) & 0xF) << 48) |
             ((uint64_t)0x0 << 52) |  // granularity = 0 for TSS
             ((uint64_t)((base >> 24) & 0xFF) << 56);

new_gdt[6] = (base >> 32) & 0xFFFFFFFFULL;
*/

// insure tss is 0'd
/*memset(&tss_entry, 0, sizeof tss_entry);
//tss_entry.ss0  = 0x10;
tss_entry.rsp0 = kernel_stack_top;  // Set the kernel stack pointer
tss_entry.io_bitmap_offset = sizeof(tss_entry);  // No I/O permission bitmap

struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) new_gdtr = {
    .limit = sizeof(new_gdt) - 1,
    .base = (uint64_t)new_gdt
};
asm volatile ("lgdt %0" :: "m"(new_gdtr));
*/

/* get shit setup */
    load_gdt();
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
    flanterm_write(flanterm_get_ctx(), "\033[38;2;0;160;255m", 18);
    /* display banner */
    flanterm_write(flanterm_get_ctx(), "    __  ___                  ___ __  __    _____ __ __", strlenn("    __  ___                  ___ __  __    _____ __ __"));
    flanterm_write(flanterm_get_ctx(), "\n", 1);
    flanterm_write(flanterm_get_ctx(), "   /  |/  /___  ____  ____  / (_) /_/ /_  / ___// // /", strlenn("   /  |/  /___  ____  ____  / (_) /_/ /_  / ___// // /"));
    flanterm_write(flanterm_get_ctx(), "\n", 1);
    flanterm_write(flanterm_get_ctx(), "  / /|_/ / __ \\/ __ \\/ __ \\/ / / __/ __ \\/ __ \\/ // /_", strlenn("  / /|_/ / __ \\/ __ \\/ __ \\/ / / __/ __ \\/ __ \\/ // /_"));
    flanterm_write(flanterm_get_ctx(), "\n", 1);
    flanterm_write(flanterm_get_ctx(), " / /  / / /_/ / / / / /_/ / / / /_/ / / / /_/ /__  __/", strlenn(" / /  / / /_/ / / / / /_/ / / / /_/ / / / /_/ /__  __/"));
    flanterm_write(flanterm_get_ctx(), "\n", 1);
    flanterm_write(flanterm_get_ctx(), "/_/  /_/\\____/_/ /_/\\____/_/_/\\__/_/ /_/\\____/  /_/   ", strlenn("/_/  /_/\\____/_/ /_/\\____/_/_/\\__/_/ /_/\\____/  /_/   "));
    flanterm_write(flanterm_get_ctx(), "\n", 1);
    flanterm_write(flanterm_get_ctx(), "\033[0m", 4); // reset color
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

flanterm_write(ft_ctx, "\033[32m", 5);
flanterm_write(flanterm_get_ctx(), "[INFO] CPU is ", 14);
flanterm_write(flanterm_get_ctx(), get_model(), 12);
flanterm_write(flanterm_get_ctx(), "!\n", 3);

bool hypervisor = is_running_under_hypervisor();
if (hypervisor) {
    flanterm_write(ft_ctx, "[INFO] System is running under a hypervisor!\n", 46);
} else {
    flanterm_write(ft_ctx, "[INFO] System is not running under a hypervisor.\n", 50);
}


flanterm_write(flanterm_get_ctx(), "\033[33m", 5);
flanterm_write(flanterm_get_ctx(), "[INFO] Press [F1] for ACPI sleep state S5 (Shutdown)\n", 56);
flanterm_write(flanterm_get_ctx(), "[INFO] Press [F2] for current date and time info from RTC\n", 62);

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
setup_linebuffer();
initialise_multitasking();
// We're done, just hang...
    hcf();
}


