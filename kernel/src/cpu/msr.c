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