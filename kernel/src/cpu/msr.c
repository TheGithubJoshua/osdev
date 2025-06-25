#include <stdint.h>
#include <cpuid.h>
#include "cpuid.h"
#include "msr.h"

const uint32_t CPUID_FLAG_MSR = 1 << 5;

bool has_msr() {
	static uint32_t a, such, empty, d; // eax, edx
	__get_cpuid(1, &a, &such, &empty, &d);
	return d & CPUID_FLAG_MSR;
}

void get_msr(uint32_t msr, uint32_t *lo, uint32_t *hi) {
	asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

void set_msr(uint32_t msr, uint32_t lo, uint32_t hi) {
	asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

const char* get_model(void) {
    unsigned int eax, ebx, ecx, edx;
    static char vendor[13]; // Static to persist after function returns

    __asm__ volatile (
        "cpuid"
        : "=b" (ebx), "=d" (edx), "=c" (ecx)
        : "a" (0)
    );

    *(unsigned int*)&vendor[0] = ebx;
    *(unsigned int*)&vendor[4] = edx;
    *(unsigned int*)&vendor[8] = ecx;
    vendor[12] = '\0';

    return vendor;
}

bool is_running_under_hypervisor(void) {
    unsigned int eax, ebx, ecx, edx;

    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        // Bit 31 of ECX set indicates hypervisor present
        return (ecx & CPUID_FEAT_ECX_HYPERVISOR) != 0;
    }

    // CPUID not supported or failed
    return false;
}
