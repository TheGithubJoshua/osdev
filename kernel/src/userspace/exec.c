#include "exec.h"
#include "../mm/pmm.h"
#include "enter.h"
#include "../elf/elf.h"
#include "../fs/fs.h"
#include "../thread/thread.h"
#include "../syscall/syscall.h"
#include "../iodebug.h"
#include "../ff16/source/ff.h"
#include "../memory.h"

char* path2;
int open_flags2;
mode_t mode2;
int fuckyou2;

void openfile2() { // why
    // open
    serial_puts("path(syscall): ");
    serial_puts(path2);
    fuckyou2 = open(path2, open_flags2, mode2);
    task_exit();
}

/* takes a path, reads it's contents into memory, maps it into a random, free, lower-half address and returns said address */
uint64_t load_program(int filedesc) {
	unsigned int size;
	char *buf;
	size_t br;
	uint64_t user_code_vaddr;

	// Get file size
	size = get_size(filedesc);

	// Allocate memory
	buf = palloc((size + PAGE_SIZE - 1) / PAGE_SIZE, true);
	if (!buf) {
	    serial_puts("palloc failed\n");
	    return -1;
	}

	// Read into buffer
	br = read(filedesc, buf, size);
	if (br != size) {
	    serial_puts("read failed or incomplete\n");
	    return -1;
	}

	/* Now buf contains the entire file */
	serial_puts("File loaded into memory\n");
	//fd = fat_read(fn, 0);
	entry_t elf = load_elf(buf, false);
	pfree(buf, (size + PAGE_SIZE - 1) / PAGE_SIZE);
	serial_puts("elf size: ");
	serial_puthex(elf_size);
	//elf_size += 0x2000; // fix me
	// Map the page to userspace address, readable + executable + user access
	void* phys_page = palloc((elf_size + PAGE_SIZE - 1) / PAGE_SIZE, false); // alloc physical // to future me: memory allocator is broken here and returns higher-half memory that is filled with something that resembles the libctest. update: I ripped out the whole fucking memory allocator :)
// to future me: after ripping out the memory allocator and replacing it with one that worked properly so I could use bigger stack, the 8MiB stack just chnaged the faulting address, so prolly faults something on stack idk todo: test
	// start virtual address of user code
	user_code_vaddr = find_address(elf_size);
	serial_puts("user_code_vaddr: ");
	serial_puthex(user_code_vaddr);
	// Then map it into user space for execution
	map_len(read_cr3(),
	        user_code_vaddr,
	        (uint64_t)phys_page,
	        PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE | PAGE_EXECUTE, elf_size);

	// Now it's safe to memcpy
	//memcpy((void*)user_code_vaddr, (void*)elf, elf_size);

	return (uint64_t)elf;
}