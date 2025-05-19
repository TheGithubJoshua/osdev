#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "iodebug.h"
#include "interrupts.h"
#include "acpi.h"
#include "timer.h"
#include "fb.h"
#include "flanterm/flanterm.h"
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

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

// im putting io here bc im lazy
// not anymore

// Render the glyph at position (x, y) with foreground color `fg_color`
/*void draw(uint32_t x, uint32_t y, uint8_t glyph[16], uint32_t fg_color) {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    for (uint32_t row = 0; row < 16; row++) {
        uint8_t row_data = glyph[row];
        for (uint32_t col = 0; col < 8; col++) {
            // Check if the bit is set (pixel should be colored)
            if (row_data & (0x80 >> col)) {
                uint32_t *pixel = (uint32_t*)((uint8_t*)fb->address + (y + row) * fb->pitch + (x + col) * 4);
                *pixel = fg_color; // Set pixel to foreground color (e.g., white)
            }
        }
    }
}

void draw_text(uint32_t x, uint32_t y, const char *text, uint32_t fg_color) {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t original_x = x; // For newline handling
    uint32_t max_x = fb->width - 8; // Right edge (8px char width)
    uint32_t max_y = fb->height - 16; // Bottom edge (16px char height)

    while (*text && y <= max_y) {
        switch (*text) {
            case '\n': // Newline
                x = original_x;
                y += 16;
                break;
                
            case '\t': // Tab (4 spaces)
                x = original_x + ((x - original_x + 32) / 32) * 32;
                if (x > max_x) {
                    x = original_x;
                    y += 16;
                }
                break;
                
            case '\r': // Carriage return
                x = original_x;
                break;
                
            default:
                if (*text >= 0x20 && *text <= 0x7F) { // Printable ASCII
                    if (x <= max_x) {
                        draw(x, y, font[(uint8_t)*text], fg_color);
                        x += 8;
                    }
                }
                // Non-printable chars are ignored
                break;
        }
        
        // Handle line wrapping
        if (x > max_x) {
            x = original_x;
            y += 16;
        }
        
        text++;
    }
}
*/

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
    struct flanterm_context *ft_ctx = NULL;
    
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


    // We're done, just hang...
//while (true)
    hcf();
}


