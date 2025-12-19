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

uintptr_t setup_stack(void *stack_top, char **argvp, char** envp) {
    char *sp = (char *)stack_top;
    int argc = 0;
    int envc = 0;

    // 1. Calculate counts
    while (argvp[argc]) argc++;
    while (envp[envc]) envc++;

    // 2. Prepare temp arrays to hold the *final* stack addresses of strings.
    // We use VLA (Variable Length Arrays) here for simplicity. 
    uintptr_t argv_addrs[argc];
    uintptr_t envp_addrs[envc];

    // 3. Copy ENVP strings to stack (High Address)
    // We iterate forward, but the stack grows down. 
    for (int i = 0; i < envc; ++i) {
        size_t len = strlenn(envp[i]) + 1;
        sp -= len;                 // Move stack down by bytes
        memcpy(sp, envp[i], len);  // Copy string
        envp_addrs[i] = (uintptr_t)sp; // Save this address for later
    }

    // 4. Copy ARGV strings to stack
    for (int i = 0; i < argc; ++i) {
        size_t len = strlenn(argvp[i]) + 1;
        sp -= len;
        memcpy(sp, argvp[i], len);
        argv_addrs[i] = (uintptr_t)sp; // Save this address for later
    }

    // 5. Align the stack pointer to 16 bytes
    // (This ensures the string block ends at a 16-byte boundary)
    sp = (void *)ALIGN_DOWN((uintptr_t)sp, 16);

    // 6. Padding for alignment
    // The final structure we push is:
    // argc(1) + argv(argc) + NULL(1) + envp(envc) + NULL(1)
    // Total 8-byte slots = 3 + argc + envc.
    // If total slots is odd, RSP will end up 8-byte aligned but not 16-byte aligned.
    // We want RSP to be 16-byte aligned at the end.
    if (((3 + argc + envc) & 1) != 0) {
        sp -= 8; // Add a padding slot
    }

    // 7. Setup Pointers
    // Switch to uintptr_t pointer arithmetic for writing 8-byte values
    uintptr_t *usp = (uintptr_t *)sp;

    // Push NULL (End of envp)
    *(--usp) = 0;

    // Push envp pointers (Reverse order so envp[0] is lowest address)
    for (int i = envc - 1; i >= 0; --i) {
        *(--usp) = envp_addrs[i];
    }

    // Push NULL (End of argv)
    *(--usp) = 0;

    // Push argv pointers
    for (int i = argc - 1; i >= 0; --i) {
        *(--usp) = argv_addrs[i];
    }

    // Push argc
    STACK_PUSH(usp, argc);

    // 8. Return new Stack Pointer
    if (((uintptr_t)usp % 16) == 0) {
        usp -= 1; // push an extra 8-byte slot for padding
    }
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