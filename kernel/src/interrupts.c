#include <stdbool.h>
#include <stdint.h>
#include "iodebug.h"
// TODO: when returning from interrupt (fault) skip over faulting proccess after restore.
// TODO: check if iretq just returns to start of handling interrupt.
#define GDT_OFFSET_KERNEL_CODE 0x28

typedef struct {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t	    ist;          // The IST in the TSS that the CPU will load into RSP; set to zero for now
	uint8_t     attributes;   // Type and attributes; see the IDT page
	uint16_t    isr_mid;      // The higher 16 bits of the lower 32 bits of the ISR's address
	uint32_t    isr_high;     // The higher 32 bits of the ISR's address
	uint32_t    reserved;     // Set to zero
} __attribute__((packed)) idt_entry_t;

__attribute__((aligned(0x10))) 
static idt_entry_t idt[256]; // Create an array of IDT entries; aligned for performance

typedef struct {
	uint16_t	limit;
	uint64_t	base;
} __attribute__((packed)) idtr_t;

static idtr_t idtr; // IDTR

/*struct cpu_status_t {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rsp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vector_number;
    uint64_t error_code;
    uint64_t iret_rip;
    uint64_t iret_cs;
    uint64_t iret_flags;
    uint64_t iret_ss;
};
*/
//__attribute__((noreturn))
//struct cpu_status_t exception_handler(struct cpu_status_t);
void exception_handler(uint64_t vector);
//struct cpu_status_t exception_handler(struct cpu_status_t context) {
void exception_handler(uint64_t vector) {
    //__asm__ volatile ("cli; hlt"); // Completely hangs the computer
    switch (vector) {
    case 0:
        serial_puts("divide error (retard tried to divide by zero)");
        break;
    case 1:
        serial_puts("debug exception");
        break;
    case 2:
        serial_puts("nmi interrupt");
        break;
    case 3:
        serial_puts("breakpoint (stop and have a break)");
        break;
    case 4:
        serial_puts("overflow");
        break;
    case 5:
        serial_puts("bound range exceeded");
        break;
    case 6:
        serial_puts("invalid opcode");
        break;
    case 7:
        serial_puts("device not available");
        break;
    case 8:
        serial_puts("double fault");
        break;
    case 9:
        serial_puts("coprocessor segment overrun");
        break;
    case 10:
        serial_puts("invalid tss");
        break;
    case 11:
        serial_puts("segment not present");
        break;
    case 12:
        serial_puts("stack-segment fault");
        break;
    case 13:
        serial_puts("general protection fault");
        break;
    case 14:
        serial_puts("page fault");
        break;
    case 15:
        serial_puts("reserved");
        break;
    case 16:
        serial_puts("floating-point error");
        break;
    case 17:
        serial_puts("alignment check");
        break;
    case 18:
        serial_puts("machine check");
        break;
    case 19:
        serial_puts("simd floating-point exception");
        break;
    case 20:
        serial_puts("virtualization exception");
        break;
    case 21:
        serial_puts("control protection exception");
        break;
    case 22:
        serial_puts("reserved");
        break;
    case 32:
        serial_puts("external interrupt");
        break;
    default:
        serial_puts("unknown interrupt");
        break;
    }

    //return context;
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = GDT_OFFSET_KERNEL_CODE;
    descriptor->ist            = 0;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved       = 0;
}
const int IDT_MAX_DESCRIPTORS = 256;

static bool vectors[256];

extern void* isr_stub_table[];

void idt_init() {
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * 256 - 1;

    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        vectors[vector] = true;
    }

    __asm__ volatile ("lidt %0" : : "m"(idtr)); // load the new IDT
    __asm__ volatile ("sti"); // set the interrupt flag
}
