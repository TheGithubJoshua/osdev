#include "exec.h"
#include "../mm/pmm.h"
#include "enter.h"
#include "../elf/elf.h"
#include "../fs/fs.h"
#include "../thread/thread.h"
#include "../syscall/syscall.h"
#include "../errno.h"
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

uintptr_t setup_stack(void *stack_top, char **argvp, char **envp)
{
    char *sp = stack_top;
    int argc = 0, envc = 0;

    while (argvp && argvp[argc]) argc++;
    while (envp && envp[envc]) envc++;

    uintptr_t argv_addrs[argc];
    uintptr_t envp_addrs[envc];

    // Copy envp strings
    for (int i = envc - 1; i >= 0; --i) {
        size_t len = strlenn(envp[i]) + 1;
        sp -= len;
        memcpy(sp, envp[i], len);
        envp_addrs[i] = (uintptr_t)sp;
    }

    // Copy argv strings
    for (int i = argc - 1; i >= 0; --i) {
        size_t len = strlenn(argvp[i]) + 1;
        sp -= len;
        memcpy(sp, argvp[i], len);
        argv_addrs[i] = (uintptr_t)sp;
    }

    // Align down BEFORE pushing pointers
    sp = (char *)((uintptr_t)sp & ~0xF);

    uintptr_t *usp = (uintptr_t *)sp;

    // AUXV
    *(--usp) = 0; // AT_NULL value
    *(--usp) = 0; // AT_NULL key

    // envp NULL
    *(--usp) = 0;
    for (int i = envc - 1; i >= 0; --i)
        *(--usp) = envp_addrs[i];

    // argv NULL
    *(--usp) = 0;
    for (int i = argc - 1; i >= 0; --i)
        *(--usp) = argv_addrs[i];

    // argc
    *(--usp) = argc;

    if (((uintptr_t)usp & 0xF) != 8)
        usp--;

    return (uintptr_t)usp;
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
	    return -ENOMEM;
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
	//memcpy((void*)user_code_vaddr, (void*)elf, elf_size);

	return (uint64_t)elf;
}