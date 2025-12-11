#include "liballoc.h"
#include "../thread/thread.h"
#include "../panic/panic.h"
#include "pmm.h"
#include "vmm.h"

static int page_size = PAGE_SIZE;
spinlock_t alloc_lock;

int liballoc_lock() {
	acquire(&alloc_lock);
	return 0;
}

int liballoc_unlock() {
	release(&alloc_lock);
	return 0;
}

void* liballoc_alloc(int pages) {
	unsigned int size = pages * page_size;

	void* mem = vmm_alloc(size, VM_FLAG_WRITE, NULL);
	if (mem == 0) { panik("Memory allocation failure!"); }

	return mem;
}

int liballoc_free(void* ptr, int pages) {
	vm_free(ptr);
}