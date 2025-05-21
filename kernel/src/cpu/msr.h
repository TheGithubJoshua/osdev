#include <stdint.h>
#include <stdbool.h>

bool has_msr();
void get_msr(uint32_t msr, uint32_t *lo, uint32_t *hi);
void set_msr(uint32_t msr, uint32_t lo, uint32_t hi);
