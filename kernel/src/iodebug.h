#include <stdint.h>

void serial_init();
void serial_putc(char c);
void serial_puts(const char *s);
void serial_puthex(uint64_t value);

void outb(uint16_t port, uint8_t val);
void io_wait();
uint8_t inb(uint16_t port);