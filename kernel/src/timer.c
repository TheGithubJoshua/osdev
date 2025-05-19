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
void timer_wait(int ticks)
{
    volatile unsigned int eticks;

    eticks = timer_ticks + ticks;
    while(timer_ticks < eticks) 
    {
        __asm__ __volatile__ ("sti\n\t"
                      "hlt\n\t"
                      "cli");
    }
    __asm__ __volatile__ ("sti");
}

// play sounnd with PC speaker
void play_sound(uint32_t freq) {
    uint32_t Div;
    uint8_t tmp;

    // set PIT to the desired frequency
    Div = 1193180 / freq;
    outb(0x43, 0xb6);
    outb(0x42, (uint8_t) (Div) );
    outb(0x42, (uint8_t) (Div >> 8));

    // play the sound
    tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

// make it shut up
void nosound() {
    uint8_t tmp = inb(0x61) & 0xFC;
    
    outb(0x61, tmp);
}

// make a beep
void beep() {
     play_sound(440);
     timer_wait(20);
     nosound();
     //set_PIT_2(old_frequency);
}
