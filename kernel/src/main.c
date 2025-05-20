#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "iodebug.h"
#include "interrupts.h"
#include "acpi/acpi.h"
#include "mm/pmm.h"
#include "timer.h"
#include "fb.h"
#include "memory.h"
//#include "liballoc/liballoc.h"
#include "flanterm/backends/fb.h"
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
        flanterm_write(ft_ctx, "palloc memory test failed: value mismatch\n", 38);
    } else {
        flanterm_write(ft_ctx, "palloc memory test passed\n", 27);
        pfree(ptr, 1);
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
    timer_phase(100); // 100 Hz
    //timer_handler();

    rsdp_t *rsdp = get_acpi_table();
    struct xsdt_t *xsdt = get_xsdt_table();
    get_fadt(get_xsdt_table());

    init_flanterm();
    struct flanterm_context *ft_ctx = flanterm_get_ctx();
    pmm_init();
    
    beep();
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
//    for (size_t i = 0; i < 100; i++) {
//        volatile uint32_t *fb_ptr = framebuffer->address;
//        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
//    }

// demo fault
asm("int $0");

serial_puts("hello \n");
flanterm_write(ft_ctx, "Welcome\n", 9);

test_palloc();

// We're done, just hang...
    hcf();
}


