#include "panic.h"
#include "../memory.h"
#include "../util/fb.h"

#define PHEX(name, val) \
    do { \
        flanterm_write(flanterm_get_ctx(), name " = 0x", sizeof(name " = 0x") - 1); \
        flanterm_puthex((uint64_t)(val)); \
        flanterm_write(flanterm_get_ctx(), "\n", 1); \
    } while(0)

#define READ_CR0() ({ uint64_t val; asm volatile("mov %%cr0, %0" : "=r"(val)); val; })
#define READ_CR2() ({ uint64_t val; asm volatile("mov %%cr2, %0" : "=r"(val)); val; })
#define READ_CR3() ({ uint64_t val; asm volatile("mov %%cr3, %0" : "=r"(val)); val; })
#define READ_CR4() ({ uint64_t val; asm volatile("mov %%cr4, %0" : "=r"(val)); val; })

static const char *exceptions[] = {
    "divide error (retard tried to divide by zero)",
    "debug exception",
    "nmi interrupt",
    "breakpoint (stop and have a break)",
    "overflow",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid tss",
    "segment not present",
    "stack-segment fault",
    "general protection fault",
    "page fault",
    "reserved",
    "floating-point error",
    "alignment check",
    "machine check",
    "simd floating-point exception",
    "virtualization exception",
    "control protection exception",
    "reserved"
};

void kpanik(cpu_status_t* cpu) {
	int exception = cpu->vector_number;

	flanterm_write(flanterm_get_ctx(), "\033[2J\033[H", sizeof("\033[2J\033[H"));
	const char *banner =
	" ______     __  __     __     __         __            __     ______     ______     __  __     ______    \n"
	"/\\  ___\\   /\\ \\/ /    /\\ \\   /\\ \\       /\\ \\          /\\ \\   /\\  ___\\   /\\  ___\\   /\\ \\/\\ \\   /\\  ___\\   \n"
	"\\ \\___  \\  \\ \\  _\"-.  \\ \\ \\  \\ \\ \\____  \\ \\ \\____     \\ \\ \\  \\ \\___  \\  \\ \\___  \\  \\ \\ \\_\\ \\  \\ \\  __\\   \n"
	" \\/\\_____\\  \\ \\_\\ \\_\\  \\ \\_\\  \\ \\_____\\  \\ \\_____\\     \\ \\_\\  \\/\\_____\\  \\/\\_____\\  \\ \\_____\\  \\ \\_____\\ \n"
	"  \\/_____/   \\/_/\\/_/   \\/_/   \\/_____/   \\/_____/      \\/_/   \\/_____/   \\/_____/   \\/_____/   \\/_____/ \n";
	flanterm_write(flanterm_get_ctx(), "\033[91m", strlenn("\033[91m"));

	flanterm_write(flanterm_get_ctx(), banner, strlenn(banner));
	flanterm_write(flanterm_get_ctx(), "\n", 1);

	flanterm_write(flanterm_get_ctx(), "Kernel Panic! A fatal exception occured; CPU halted.", strlenn("Kernel Panic! A fatal exception occured; CPU halted."));
	flanterm_write(flanterm_get_ctx(), "\n", 1);
	flanterm_write(flanterm_get_ctx(), "\n", 1);
	flanterm_write(flanterm_get_ctx(), "\033[0m", 4);   // reset color
	flanterm_write(flanterm_get_ctx(), "\n", 1);

	flanterm_write(flanterm_get_ctx(), "Fault type: ", sizeof("Fault type: "));
	flanterm_write(flanterm_get_ctx(), exceptions[exception], strlenn(exceptions[exception]));
	flanterm_write(flanterm_get_ctx(), "\n", 1);

	flanterm_write(flanterm_get_ctx(), "Error code: ", strlenn("Error code: "));
	flanterm_write(flanterm_get_ctx(), "0x", 3);
	flanterm_puthex((cpu->error_code));
	flanterm_write(flanterm_get_ctx(), "\n", 1);
	flanterm_write(flanterm_get_ctx(), "\n", 1);

	flanterm_write(flanterm_get_ctx(), "-- Regsiters --", strlenn("-- Regsiters ---"));
	flanterm_write(flanterm_get_ctx(), "\n", 1);
	PHEX("RAX", cpu->rax);
	PHEX("RBX", cpu->rbx);
	PHEX("RCX", cpu->rcx);
	PHEX("RDX", cpu->rdx);
	PHEX("RSI", cpu->rsi);
	PHEX("RDI", cpu->rdi);
	PHEX("RBP", cpu->rbp);
	PHEX("R8 ", cpu->r8);
	PHEX("R9 ", cpu->r9);
	PHEX("R10", cpu->r10);
	PHEX("R11", cpu->r11);
	PHEX("R12", cpu->r12);
	PHEX("R13", cpu->r13);
	PHEX("R14", cpu->r14);
	PHEX("R15", cpu->r15);
	PHEX("RIP", cpu->iret_rip);
	PHEX("CS ", cpu->iret_cs);
	PHEX("RFLAGS", cpu->iret_flags);
	PHEX("RSP", cpu->iret_rsp);
	PHEX("SS ", cpu->iret_ss);
	PHEX("CR0", READ_CR0());
	PHEX("CR2", READ_CR2());
	PHEX("CR3", READ_CR3());
	PHEX("CR4", READ_CR4());

	// hang
	asm volatile ("cli");
	asm volatile ("hlt");
}