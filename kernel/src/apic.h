#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>

bool check_apic();
void set_apic_base(uintptr_t apic);
uintptr_t get_apic_base();
void enable_apic();

volatile uint32_t* apic_mmio();
void apic_write(uint32_t reg, uint32_t val);
uint32_t apic_read(uint32_t reg);
