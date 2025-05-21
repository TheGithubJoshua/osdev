#include <stdint.h>
#include <cpuid.h>
#include "cpu/msr.h"
#include "iodebug.h"
#include "memory.h"

#define CPUID_FEAT_EDX_APIC (1 << 9)
#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800

// returns true if local apic is supported and enabled.
bool check_apic() {
	static uint32_t a, such, empty, d;
	__get_cpuid(1, &a, &such, &empty, &d);
	return d & CPUID_FEAT_EDX_APIC;
}

// set physical address for local apic registers.
void set_apic_base(uintptr_t apic) {
	uint32_t d = 0;
	uint32_t a = (apic & 0xfffff0000) | IA32_APIC_BASE_MSR_ENABLE;

#ifdef __PHYSICAL_MEMORY_EXTENSION__
	d = (apic >> 32) & 0x0f;
#endif

	set_msr(IA32_APIC_BASE_MSR, a, d);
}

uintptr_t get_apic_base() {
   uint32_t eax, edx;
   get_msr(IA32_APIC_BASE_MSR, &eax, &edx);

#ifdef __PHYSICAL_MEMORY_EXTENSION__
   return (eax & 0xfffff000) | ((edx & 0x0f) << 32);
#else
   return (eax & 0xfffff000);
#endif
}

void enable_apic() {
	set_apic_base(get_apic_base());
	outd(0xF0, ind(0xF0) | 0x100);
}

volatile uint32_t* apic_mmio() {
    return (volatile uint32_t *)((uintptr_t)get_apic_base() + get_phys_offset());
}

void apic_write(uint32_t reg, uint32_t val) {
    apic_mmio()[reg >> 2] = val;
}

uint32_t apic_read(uint32_t reg) {
    return apic_mmio()[reg >> 2];
}