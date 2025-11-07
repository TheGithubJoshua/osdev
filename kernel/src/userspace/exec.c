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
	FIL fil;
	FRESULT fr;
	UINT br;
	unsigned int size;
	char *fd;

	uint64_t user_code_vaddr;

	/* Open the file */
	/*fr = f_open(&fil, fn, FA_READ);
	if (fr != FR_OK) {
	    serial_puts("f_open failed\n");
	    serial_puthex(fr);
	    return -1;
	}*/
	//fr = open(fn, FA_READ, FA_READ);
	fr = (int)filedesc;

	/* Get file size */
	size = get_size(fr);

	/* Allocate buffer for the file */
	fd = palloc((size + PAGE_SIZE - 1) / PAGE_SIZE, true);
	if (!fd) {
	    serial_puts("palloc failed\n");
	    f_close(&fil);
	    //return -1;
	}

	/* Read the entire file */
	//fr = f_read(&fil, fd, size, &br);
	br = read(fr, fd, size);
	if (br != size) {
	    serial_puts("f_read failed or incomplete\n");
	    serial_puthex(fr);
	    f_close(&fil);
	    return -1;
	}

	/* Close file */
	f_close(&fil);

	/* Now buf contains the entire file */
	serial_puts("File loaded into memory\n");
	//fd = fat_read(fn, 0);
	entry_t elf = load_elf(fd, false);
	serial_puts("elf size: ");
	serial_puthex(elf_size);
	//elf_size += 0x2000; // fix me
	// Map the page to userspace address, readable + executable + user access
	void* phys_page = palloc((elf_size + PAGE_SIZE - 1) / PAGE_SIZE, false); // alloc physical

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
	memcpy((void*)user_code_vaddr, (void*)elf, elf_size);

	return user_code_vaddr;
}