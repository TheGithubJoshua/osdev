#include "/home/joshua/instant-gratification/kernel/src/lai/include/lai/host.h"
#include "../iodebug.h"
#include "../mm/pmm.h"
#include "../memory.h"
#include "../timer.h"
#include "../fb.h"
#include "acpi.h"

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
	    return palloc(newsizepg, true);
	}
	if (newsize == 0)
	{
	    pfree(ptr, true);
	    return 0;
	}
	void *ret = palloc(newsizepg, true);
	memcpy(ret, ptr, oldsize);
	pfree(ptr, oldsizepg);
	return ret;
}

void laihost_free(void *ptr, size_t size) {
	pfree(ptr, (size + PAGE_SIZE - 1) / PAGE_SIZE);
}

void *laihost_map(size_t address, size_t count) {
	return (void *)(address + get_phys_offset());
}

void laihost_unmap(void *address, size_t count) {
	// wow such empty
}

void *laihost_scan(const char *sig, size_t count) {
	return get_table(sig, count);
}

void laihost_outb(uint16_t port, uint8_t val) { outb(port, val); }
void laihost_outw(uint16_t port, uint16_t val) { outb(port, val); }
void laihost_outd(uint16_t port, uint32_t val) { outb(port, val); }

uint8_t laihost_inb(uint16_t port) { return inb(port); }
uint16_t laihost_inw(uint16_t port) { return inb(port); }
uint32_t laihost_ind(uint16_t port) { return inb(port); }

// TODO: PCI

void laihost_sleep(uint64_t ms) { timer_wait(ms * 10); }


