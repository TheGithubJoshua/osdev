#include <stdint.h>

void idt_init(void);
void irq_unmask_all();
void irq_remap();
void irq_handler(uint64_t vector);
void init_pit(uint32_t frequency);