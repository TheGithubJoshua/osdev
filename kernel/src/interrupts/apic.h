#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>

#define IOREGSEL 0xFEC00000
#define IOREGWIN 0xFEC00010
#define IOAPIC_VIRT_ADDR ((uintptr_t)get_ioapic_addr() + get_phys_offset())

bool check_apic();
void set_apic_base(uintptr_t apic);
uintptr_t get_apic_base();
void enable_apic();

volatile uint32_t* apic_mmio();
void apic_write(uint32_t reg, uint32_t val);
uint32_t apic_read(uint32_t reg);
volatile uint32_t* apic_mmio();
void apic_write(uint32_t reg, uint32_t val);
uint32_t apic_read(uint32_t reg);
volatile uint32_t* ioapic_mmio();
void ioapic_write(uint32_t reg, uint32_t val);
uint32_t ioapic_read(uint32_t reg);

void write_ioapic_register(const uintptr_t apic_base, const uint8_t offset, const uint32_t val);
uint32_t read_ioapic_register(const uintptr_t apic_base, const uint8_t offset);