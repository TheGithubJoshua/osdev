#include "iodebug.h"
#include "interrupts/apic.h"
#include "timer.h"
#include "thread/thread.h"
#include <stdint.h>

volatile int timer_ticks = 0;
volatile bool preempt_pending;

void timer_phase(int hz) {
    int divisor = 1193180 / hz;   /* Calculate our divisor */
    outb(0x43, 0x36);             /* Set our command byte 0x36 */
    outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outb(0x40, divisor >> 8);     /* Set high byte of divisor */
}
	

void timer_handler() {
    timer_ticks++;
    if (timer_ticks % 10 == 0)
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

// get system uptime (in ms)
size_t uptime() {
    size_t ticks = timer_ticks;
    size_t ms = ticks * 10; // Assuming timer_ticks increments every 10ms
    return ms;
}

uint64_t get_ticks() {
    return timer_ticks;
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
     timer_wait(2);
     nosound();
     //set_PIT_2(old_frequency);
}
#define APIC_LVT_INT_MASKED (1 << 16)

void apic_start_timer() {
        // Tell APIC timer to use divider 16
        apic_write(0x3E0, 0x3);
 
        // Set APIC init counter to -1
        apic_write(0x380, 0xFFFFFFFF);

        // Prepare the PIT to sleep for 10ms (10000µs)
        timer_wait(10);
  
        // Stop the APIC timer
        apic_write(0x320, APIC_LVT_INT_MASKED);
 
        // Now we know how often the APIC timer has ticked in 10ms
        uint32_t ticksIn10ms = 0xFFFFFFFF - apic_read(0x390);
 
        // Start timer as periodic on IRQ 0, divider 16, with the number of ticks we counted
        apic_write(0x320, 32 | 0x20000);
        apic_write(0x3E0, 0x3);
        apic_write(0x380, ticksIn10ms);
        #define PIC1        0x20        /* IO base address for master PIC */
        #define PIC2        0xA0        /* IO base address for slave PIC */
        #define PIC1_COMMAND    PIC1
        #define PIC1_DATA   (PIC1+1)
        #define PIC2_COMMAND    PIC2
        #define PIC2_DATA   (PIC2+1)
        //outb(0x21, inb(0x21) | 0x01);
        // disable
        outb(PIC1_DATA, 0xff);
        outb(PIC2_DATA, 0xff);
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int char_to_digit(char c) {
    return c - '0';
}

void play_tune(const char *tune, int bpm) {
    while (*tune) {
        char note = *tune++;

        // convert uppercase to lowercase manually
        if (note >= 'A' && note <= 'Z') note += 32;

        int octave = 4;    // default octave
        int duration = 4;  // default duration (quarter note)
        uint32_t freq = 0;

        // parse octave if next char is a digit
        if (is_digit(*tune)) {
            octave = char_to_digit(*tune++);
        }

        // skip comma
        if (*tune == ',') tune++;

        // parse duration
        if (is_digit(*tune)) {
            duration = 0;
            while (is_digit(*tune)) {
                duration = duration * 10 + char_to_digit(*tune++);
            }
        }

        // map note to frequency (basic)
        switch (note) {
            case 'p': freq = 0; break; // pause
            case 'c': freq = 262; break;
            case 'd': freq = 294; break;
            case 'e': freq = 330; break;
            case 'f': freq = 349; break;
            case 'g': freq = 392; break;
            case 'a': freq = 440; break;
            case 'b': freq = 494; break;
        }

        // apply octave
        int i;
        for (i = 4; i < octave; i++) freq *= 2;
        for (i = octave; i < 4; i++) freq /= 2;

        // compute duration in ms
        int delay_ms = (60000 / bpm) * 4 / duration;

        if (freq) {
            play_sound(freq);
        } else {
            nosound();
        }

        // simple busy-wait
        for (volatile int i = 0; i < delay_ms * 10000; i++);
        nosound();

        // skip separator
        if (*tune == ';') tune++;
    }
}


