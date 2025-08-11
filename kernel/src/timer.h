#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern volatile bool preempt_pending;

void timer_handler();
void timer_phase(int hz);
void timer_wait(int ticks);
void play_sound(uint32_t freq);
void nosound();
void beep();
void apic_start_timer();
size_t uptime();
uint64_t get_ticks();