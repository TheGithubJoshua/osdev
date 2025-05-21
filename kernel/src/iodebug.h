#include <stdint.h>

void serial_init();
void serial_putc(char c);
void serial_puts(const char *s);
void serial_puthex(uint64_t value);

void outb(uint16_t port, uint8_t val);
void outw(uint16_t port, uint16_t val);
void outd(uint16_t port, uint32_t val);
void io_wait();
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t ind(uint16_t port);