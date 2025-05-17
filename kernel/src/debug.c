#include <stdint.h>
#include "iodebug.h"

// COM1 ports
#define COM1_PORT 0x3F8
#define COM1_DATA_PORT    (COM1_PORT + 0)
#define COM1_INTERRUPT    (COM1_PORT + 1)
#define COM1_LINE_CTL     (COM1_PORT + 3)
#define COM1_MODEM_CTL    (COM1_PORT + 4)
#define COM1_LINE_STATUS  (COM1_PORT + 5)

// Initialize COM1 for debugging
void serial_init() {
    // Disable interrupts
    outb(COM1_INTERRUPT, 0x00);

    // Set baud rate divisor (115200 / 9600 = 12 → divisor = 0x000C)
    outb(COM1_LINE_CTL, 0x80);  // Enable DLAB (Divisor Latch Access Bit)
    outb(COM1_PORT + 0, 0x0C);  // Divisor low byte
    outb(COM1_PORT + 1, 0x00);  // Divisor high byte

    // Configure line control: 8 data bits, no parity, 1 stop bit (8N1)
    outb(COM1_LINE_CTL, 0x03);

    // Enable FIFO, clear buffers, and set interrupt trigger level
    outb(COM1_PORT + 2, 0xC7);  // FIFO Control Register

    // Enable modem control: Assert DTR (Data Terminal Ready) and RTS (Request to Send)
    outb(COM1_MODEM_CTL, 0x0B);
}

// Send a single character to COM1
void serial_putc(char c) {
    // Wait until the Transmitter Holding Register is empty (bit 5 of LSR)
    while ((inb(COM1_LINE_STATUS) & 0x20) == 0);
    outb(COM1_DATA_PORT, c);
}

// Send a string to COM1
void serial_puts(const char *s) {
    while (*s) {
        serial_putc(*s++);
    }
}

void serial_puthex(uint64_t value) {
    const char hex_chars[] = "0123456789ABCDEF";
    
    serial_putc('0');
    serial_putc('x');
    
    // Process 64-bit value 4 bits at a time (16 hex digits)
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (value >> i) & 0xF;
        serial_putc(hex_chars[nibble]);
    }
}