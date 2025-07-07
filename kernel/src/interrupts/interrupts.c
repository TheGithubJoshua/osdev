#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>
#include "../timer.h"
#include "../drivers/keyboard.h"
#include "../iodebug.h"
#include "interrupts.h"
#include "../memory.h"
#include "../util/fb.h"
#include "../drivers/fat/fat.h"
#include "../syscall/syscall.h"
#include "../drivers/pci/pci.h"
//#include "../drivers/ahci/ahci.h"
#include "../thread/thread.h"
#include "apic.h"

//extern volatile bool multitasking_initialized;
// TODO: when returning from interrupt (fault) skip over faulting proccess after restore.
// TODO: check if iretq just returns to start of handling interrupt.
#define GDT_OFFSET_KERNEL_CODE 0x08

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

void set_idt_entry(uint8_t vector, void* handler, uint8_t dpl) {
    uint64_t addr = (uint64_t)handler;

    idt_entry_t* entry = &idt[vector];
    entry->isr_low    = addr & 0xFFFF;
    entry->kernel_cs  = 0x08; // GDT kernel code segment selector
    entry->ist        = 0;    // Use default kernel stack (no IST switch)
    
    // Gate type: 0xE = interrupt gate (0xF = trap gate)
    // 0x80 = present
    // (dpl & 0x3) << 5 = privilege level
    entry->attributes = 0x80 | ((dpl & 0x3) << 5) | 0x0E;

    entry->isr_mid    = (addr >> 16) & 0xFFFF;
    entry->isr_high   = (addr >> 32) & 0xFFFFFFFF;
    entry->reserved   = 0;

    uint8_t *p = (uint8_t *)&idt[0x69];
    serial_puts("IDT entry attributes: ");
    serial_puthex(p[5]);  // attributes is the 6th byte in the entry struct
    serial_puts("\n");
}

/*
typedef struct cpu_status_t {
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
} cpu_status_t;
*/

//__attribute__((noreturn))
//void exception_handler(uint64_t vector);
//struct cpu_status_t exception_handler(struct cpu_status_t context) {

//cpu_status_t exception_handler(cpu_status_t* cpu_status_t);

void exception_handler(cpu_status_t* cpu_status_t) {
    uint64_t vector = cpu_status_t->vector_number;
    serial_puthex(vector);
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
        serial_puts("page fault \n");
        serial_puts("CR2 dump incoming... \n");
        uintptr_t val;
        asm volatile ("mov %%cr2, %0" : "=r"(val));
        serial_puthex(val);
        serial_puts("error code: ");
        serial_puthex(cpu_status_t->error_code);
        if (cpu_status_t->error_code == 0 || cpu_status_t->error_code == 0x2) {
        quickmap(val, val);
        flanterm_write(flanterm_get_ctx(), "\033[31m", 5);
        flanterm_write(flanterm_get_ctx(), "[KERNEL][WARN] Non-Present page identity mapped!\n", 50);
        flanterm_write(flanterm_get_ctx(), "\033[0m", 5);
    } else {
        flanterm_write(flanterm_get_ctx(), "\033[31m", 5);
        flanterm_write(flanterm_get_ctx(), "[KERNEL][FATAL] Page fault at ", 25);
        flanterm_write(flanterm_get_ctx(), (const char*)val, 20);
        flanterm_write(flanterm_get_ctx(), "!\n", 3);
        flanterm_write(flanterm_get_ctx(), "\033[0m", 5);
        asm volatile ("cli; hlt");
    }
        //asm volatile ("cli; hlt");
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
    default:
        serial_puts("unknown interrupt");
        break;
    }

    //return context;
}

#define PIC1        0x20        /* IO base address for master PIC */
#define PIC2        0xA0        /* IO base address for slave PIC */
#define PIC1_COMMAND    PIC1
#define PIC1_DATA   (PIC1+1)
#define PIC2_COMMAND    PIC2
#define PIC2_DATA   (PIC2+1)

void irq_unmask_all() {
    outb(0x21, 0); // unmask all interrupts on master PIC
    outb(0xA1, 0); // unmask all interrupts on slave PIC
}

void irq_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);
}
irq_handler_t irq_handlers[16] = {0};

void register_irq_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}
void irq_handler(cpu_status_t* cpu_status_t) {
    uint64_t vector = cpu_status_t->vector_number;
    uint64_t irq = vector - 32;
    if (irq_handlers[irq]) {
        irq_handlers[irq]();
    }

    switch (vector) {
    case 32:
        //serial_puts("IRQ 0!");
        timer_handler();
        //if (multitasking_initialized) {
        //cpu_status_t->iret_rip = (uint64_t)yield;
    //}
        //switch_task(cpu_status_t);
        if (multitasking_initialized) {
        apic_write(0xB0, 0);
        yield();
    }
        //schedule(cpu_status_t);
        break;
    case 33:
        //serial_puts("IRQ 1!");
        keyboard_handler();
        break;
    case 34:
        //serial_puts("IRQ 2!");
        break;
    case 35:
        serial_puts("IRQ 3!");
        break;
    case 36:
        serial_puts("IRQ 4!");
        break;
    case 37:
        serial_puts("IRQ 5!");
        break;
    case 38:
        serial_puts("IRQ 6!");
        break;
    case 39:
        serial_puts("IRQ 7!");
        break;
    case 40:
        serial_puts("IRQ 8!");
        break;
    case 41:
        serial_puts("IRQ 9!");
        break;
    case 42:
        serial_puts("IRQ 10!");
        break;
    case 43:
        serial_puts("IRQ 11!");
        break;
    case 44:
        serial_puts("IRQ 12!");
        break;
    case 45:
        serial_puts("IRQ 13!");
        break;
    case 46:
        serial_puts("IRQ 14!");
        break;
    case 47:
        serial_puts("IRQ 15!");
        break;
    default:
        serial_puts("Unknown IRQ!");
        break;
    }


    if (vector >= 40) {
        outb(0xA0, 0x20); // send EOI to slave PIC
    }

    outb(0x20, 0x20); // send EOI to master PIC
    apic_write(0xB0, 0);

}

void sata_irq_handler() {
    serial_puts("AHCI interrupt fired!\n");

    // Assuming you have a pointer to your HBA memory (e.g., abar)
    HBA_MEM *abar = get_ahci_base_address();

    // Read interrupt status register
    uint32_t is = abar->is;

    // Clear global interrupt status bits by writing back the bits you read
    abar->is = is;

    // For each port, check if it has an interrupt pending and clear it
    for (int i = 0; i < abar->cap & 0x1F; i++) {  // number of ports
        if (is & (1 << i)) {
            HBA_PORT *port = &abar->ports[i];
            uint32_t pis = port->is;
            port->is = pis;  // clear port interrupt status bits
        }
    }

    // You may also want to signal your main read/write code that
    // command completed (e.g., by clearing port->ci or setting a flag).
    
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

    for (uint8_t vector = 0; vector < 47; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        vectors[vector] = true;
    }

    __asm__ volatile ("lidt %0" : : "m"(idtr)); // load the new IDT
    __asm__ volatile ("sti"); // set the interrupt flag
}
