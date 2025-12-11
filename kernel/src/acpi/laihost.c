#include "../lai/include/lai/host.h"
#include "../iodebug.h"
#include "../mm/pmm.h"
#include "../memory.h"
#include "../timer.h"
#include "../util/fb.h"
#include "acpi.h"
#include <stdint.h>

void laihost_log(int level, const char *msg) {
	serial_puts("lai: ");
	serial_puts(msg);
	serial_puts("\n");
	flanterm_write(flanterm_get_ctx(), "\033[33m", 5);
	flanterm_write(flanterm_get_ctx(), msg, 55);
	flanterm_write(flanterm_get_ctx(), "\n", 1);
	flanterm_write(flanterm_get_ctx(), "\033[0m", 4);

}

__attribute__((noreturn)) void laihost_panic(const char *msg) {
	serial_puts("[LAI] ERR FATAL: ");
	serial_puts(msg);
	serial_puts("\n");
	__asm__ volatile ("cli; hlt");
}

void *laihost_malloc(size_t size) {
	palloc((size + PAGE_SIZE - 1) / PAGE_SIZE, true);
}

void *laihost_realloc(void *ptr, size_t newsize, size_t oldsize) {
	size_t newsizepg = (newsize + PAGE_SIZE - 1) / PAGE_SIZE;
	size_t oldsizepg = (oldsize + PAGE_SIZE - 1) / PAGE_SIZE;
	
	if (ptr == (void *)0)
	{
	    return (void*)palloc(newsizepg, true);
	}
	if (newsize == 0)
	{
	    pfree(ptr, true);
	    return 0;
	}
	void *ret = (void*)palloc(newsizepg, true);
	memcpy(ret, ptr, oldsize);
	pfree(ptr, oldsizepg);
	return ret;
}

void laihost_free(void *ptr, size_t size) {
	pfree(ptr, (size + PAGE_SIZE - 1) / PAGE_SIZE);
}

void *laihost_map(size_t address, size_t count) {
	//laihost_log(1, "laihost_map called!");
	if ((uintptr_t)address >= 0xFFFF800000000000ULL) {
		return (void *)(address + get_phys_offset());
	} else {
		for (uint64_t offset = 0; offset < count; offset += 0x1000) {
		    quickmap(address + offset, address + offset); // no return value
		}
		return (void *)address; // return the virtual address (assumes identity map)
	}
}

void laihost_unmap(void *address, size_t count) {
	// wow such empty
	//laihost_log(1, "laihost_unmap called!");
}

void *laihost_scan(const char *sig, size_t count) {
	return get_table(sig, count);
}

void laihost_outb(uint16_t port, uint8_t val) { outb(port, val); }
void laihost_outw(uint16_t port, uint16_t val) { outw(port, val); }
void laihost_outd(uint16_t port, uint32_t val) { outd(port, val); }

uint8_t laihost_inb(uint16_t port) { return inb(port); }
uint16_t laihost_inw(uint16_t port) { return inw(port); }
uint32_t laihost_ind(uint16_t port) { return ind(port); }

uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    (void)seg;
    outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
    uint8_t v = inb(0xCFC + (offset & 3));
    return v;
}
uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    (void)seg;
    outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
    uint16_t v = inw(0xCFC + (offset & 2));
    return v;
}
uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
    (void)seg;
    outd(0xCF8, (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfffc) | 0x80000000);
    uint32_t v = ind(0xCFC);
    return v;
}

void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off, uint8_t val) {
    (void)seg;
    outd(0xCF8, (bus << 16) | (dev << 11) | (fun << 8) | (off & 0xfffc) | 0x80000000);
    outb(0xCFC + (off & 3), val);
}
void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off, uint16_t val) {
    (void)seg;
    outd(0xCF8, (bus << 16) | (dev << 11) | (fun << 8) | (off & 0xfffc) | 0x80000000);
    outw(0xCFC + (off & 2), val);
}
void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off, uint32_t val) {
    (void)seg;
    outd(0xCF8, (bus << 16) | (dev << 11) | (fun << 8) | (off & 0xfffc) | 0x80000000);
    outd(0xCFC, val);
}

void laihost_sleep(uint64_t ms) { timer_wait(ms * 10); }

uint64_t pit_value() {
	outb(0x43, 0x00);
	uint8_t lo = inb(0x40);
	uint8_t hi = inb(0x40);
	return ((uint16_t)hi << 8) | lo;
}

uint64_t laihost_timer(void) {
    return get_ticks() * 100000;
}


