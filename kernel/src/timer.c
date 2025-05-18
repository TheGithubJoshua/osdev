#include "iodebug.h"

int timer_ticks = 0;

void timer_phase(int hz) {
    int divisor = 1193180 / hz;   /* Calculate our divisor */
    outb(0x43, 0x36);             /* Set our command byte 0x36 */
    outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outb(0x40, divisor >> 8);     /* Set high byte of divisor */
}
	

void timer_handler() {
    timer_ticks++;
    if (timer_ticks % 100 == 0)
    {
        //serial_puts("One second has passed\n");
    }
}

// wait function
void timer_wait(int ticks) {
    unsigned long eticks;

    eticks = timer_ticks + ticks;
    while(timer_ticks < eticks);
}