#include "iodebug.h"
#include <limine.h>
#include <lai/helpers/pm.h>
#include <stdarg.h>
#include "../util/fb.h"
#include "timer.h"
#include "../mm/pmm.h"
#include "../iodebug.h"
#include "buffer/buffer.h"
#include "cmos/rtc.h"
#include "flanterm/flanterm.h"
#include "keyboard.h"
#include "flanterm/backends/fb.h"
#include "font.c"

static inline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

static inline int strncmp(const char *s1, const char *s2, register size_t n)
{
  register unsigned char u1, u2;

  while (n-- > 0)
    {
      u1 = (unsigned char) *s1++;
      u2 = (unsigned char) *s2++;
      if (u1 != u2)
    return u1 - u2;
      if (u1 == '\0')
    return 0;
    }
  return 0;
}

static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        ++len;
    }
    return len;
}

// Define the buffer to store input characters
char input_buffer[BUFFER_SIZE];
volatile int buffer_index = 0;

// PS/2 Set 1 scancode to ASCII mapping for UK keyboard layout
// Index: scancode (0x00 to 0x7F)
// Each entry contains the unshifted and shifted ASCII characters

unsigned char kb_us[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', /* 9 */
  '9', '0', '-', '=', '\b', /* Backspace */
  '\t',         /* Tab */
  'q', 'w', 'e', 'r',   /* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
    0,          /* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
 '\'', '`',   0,        /* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',            /* 49 */
  'm', ',', '.', '/',   0,              /* Right shift */
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

unsigned char kb_us_shifted[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', /* 9 */
  '9', '0', '-', '=', '\b', /* Backspace */
  '\t',         /* Tab */
  'q', 'w', 'e', 'r',   /* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
    0,          /* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
 '\'', '`',   0,        /* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',            /* 49 */
  'm', ',', '.', '/',   0,              /* Right shift */
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};      

/* Handles the keyboard interrupt */
void keyboard_handler() {
    linebuf_t* in = fetch_linebuffer();
    unsigned char scancode;

    /* Read from the keyboard's data buffer */
    scancode = inb(0x60);

    /* If the top bit of the byte we read from the keyboard is
    *  set, that means that a key has just been released */
    if (scancode & 0x80)
    {
        /* You can use this one to see if the user released the
        *  shift, alt, or control keys... */
    }
    else
    {
        /* Here, a key was just pressed. Please note that if you
        *  hold a key down, you will get repeated key press
        *  interrupts. */

        /* Just to show you how this works, we simply translate
        *  the keyboard scancode into an ASCII value, and then
        *  display it to the screen. You can get creative and
        *  use some flags to see if a shift is pressed and use a
        *  different layout, or you can add another 128 entries
        *  to the above layout to correspond to 'shift' being
        *  held. If shift is held using the larger lookup table,
        *  you would add 128 to the scancode when you look for it */
        char buf[2] = {0}; // 1 char + null terminator
        buf[0] = kb_us[scancode]; // assuming this is unshifted char
        //draw_text(320, 320, buf, 0xFFA500);
        //init_flanterm();
        struct flanterm_context *ft_ctx = flanterm_get_ctx();
        if (!ft_ctx) return;
        flanterm_write(ft_ctx, buf, 1);

        if (buf[0] != '\0') {
            // Add character to buffer if it's not a null character
            lb_append(in, buf[0]);
            if (buffer_index < BUFFER_SIZE - 1) {
                input_buffer[buffer_index++] = buf[0];
            }
          }
       /* if (buf[0] == '\b') {
          lb_pop_back(in);
        }*/

        // temporary 'shell'
        /*if (buffer_index > 0 && input_buffer[buffer_index - 1] == '\n') {
            input_buffer[buffer_index] = '\0'; // Null-terminate the string
            buffer_index = 0; // Reset the buffer index

            // Process the command
            if (!strcmp(input_buffer, "clear\n")) {
                flanterm_write(ft_ctx, "\033[2J", 4); // clear screen
                flanterm_write(ft_ctx, "\033[H", 4); // move cursor home
            } else if (!strcmp(input_buffer, "help\n")) {
                flanterm_write(ft_ctx, "Available commands:\n", 30);
                flanterm_write(ft_ctx, "clear - clear the terminal\n", 28);
                flanterm_write(ft_ctx, "halt - shutdown the system\n", 27);
                flanterm_write(ft_ctx, "ls - list all files on the filesystem\n", 40);
                flanterm_write(ft_ctx, "cat - print the contents of a file\n", 40);
            } else if (!strcmp(input_buffer, "halt\n")) {
                flanterm_write(flanterm_get_ctx(), "\033[33m", 5);
                flanterm_write(ft_ctx, "[ACPI] Shutting down system...\n", 27);
                flanterm_write(ft_ctx, "System is going down!", 22);
                //timer_wait(50);
                lai_enter_sleep(5);
            } else if (!strcmp(input_buffer, "ls\n")) {
                FileListNode *files = fat_list_all_files();
                FileListNode *current = files;
                while (current != NULL) {
                    flanterm_write(ft_ctx, current->filename, sizeof(current->filename));
                    flanterm_write(ft_ctx, " ", 1);
                    FileListNode *next = current->next;
                    pfree(current, (sizeof(FileListNode) + PAGE_SIZE - 1) / PAGE_SIZE); // Free the memory allocated for each node
                    current = next;
                }
                flanterm_write(ft_ctx, "\n", 1);
               } else if (!strncmp(input_buffer, "cat ", 4)) {
                   // Extract the filename manually from the input buffer
                   char filename[256];
                   int i = 0;
                   const char *src = input_buffer + 4;

                   // Copy characters until space, newline, or null terminator
                   while (*src && *src != ' ' && *src != '\n' && i < sizeof(filename) - 1) {
                       filename[i++] = *src++;
                   }
                   filename[i] = '\0';
                   char *fd = fat_read(filename, 0);
                   flanterm_write(ft_ctx, fd, strlen(fd));

            } else if (!strcmp(input_buffer, " ") || !strcmp(input_buffer, "\n")) { // nothing
            } else {
                flanterm_write(ft_ctx, "\033[31m", 5);
                flanterm_write(ft_ctx, "Unknown command: ", 26);
                flanterm_write(ft_ctx, input_buffer, strlen(input_buffer));
                flanterm_write(ft_ctx, "\033[0m", 5);
            }
    }*/
        // special keys
        switch (scancode) {
        case 0x48:
          flanterm_write(ft_ctx, "\033[A", 4);
          break;
        case 0x50:
          flanterm_write(ft_ctx, "\033[B", 4);
        case 0x4B:
          flanterm_write(ft_ctx, "\033[D", 4);
          break;
        case 0x4D:
          flanterm_write(ft_ctx, "\033[C", 4);
          break;
        case 0x0E:
          flanterm_write(ft_ctx, "\x1B[P", 4);
          break;
        case 0x3B:
          flanterm_write(flanterm_get_ctx(), "\033[33m", 5);
          flanterm_write(ft_ctx, "[ACPI] Shutting down system...\n", 27);
          flanterm_write(ft_ctx, "System is going down!", 22);
          //timer_wait(50);
          lai_enter_sleep(5);
        case 0x3C:
          rtc_t rtc = read_rtc();

          // Format HR:MIN:SEC-DD-MM-YYYY
          // Format each component as two-digit strings with leading zeros if necessary
          char hr_str[3];
          if (rtc.hour < 10) {
              hr_str[0] = '0';
          } else {
              hr_str[0] = rtc.hour / 10 + '0';
          }
          hr_str[1] = rtc.hour % 10 + '0';
          hr_str[2] = '\0';

          char min_str[3];
          if (rtc.minute < 10) {
              min_str[0] = '0';
          } else {
              min_str[0] = rtc.minute / 10 + '0';
          }
          min_str[1] = rtc.minute % 10 + '0';
          min_str[2] = '\0';

          char sec_str[3];
          if (rtc.second < 10) {
              sec_str[0] = '0';
          } else {
              sec_str[0] = rtc.second / 10 + '0';
          }
          sec_str[1] = rtc.second % 10 + '0';
          sec_str[2] = '\0';

          char day_str[3];
          if (rtc.day < 10) {
              day_str[0] = '0';
          } else {
              day_str[0] = rtc.day / 10 + '0';
          }
          day_str[1] = rtc.day % 10 + '0';
          day_str[2] = '\0';

          char month_str[3];
          if (rtc.month < 10) {
              month_str[0] = '0';
          } else {
              month_str[0] = rtc.month / 10 + '0';
          }
          month_str[1] = rtc.month % 10 + '0';
          month_str[2] = '\0';

          // Calculate the full year
          int full_year = (rtc.century * 100) + rtc.year;
          char year_str[5];
          year_str[0] = (full_year / 1000) + '0';
          year_str[1] = (full_year % 1000 / 100) + '0';
          year_str[2] = (full_year % 100 / 10) + '0';
          year_str[3] = full_year % 10 + '0';
          year_str[4] = '\0';

          // Manually construct the time string
          char time_str[20];
          int index = 0;

          // Add hours and colon
          time_str[index++] = hr_str[0];
          time_str[index++] = hr_str[1];
          time_str[index++] = ':';
          index += 1; // Increment after adding each character

          // Add minutes and colon
          time_str[index++] = min_str[0];
          time_str[index++] = min_str[1];
          time_str[index++] = ':';
          index += 1;

          // Add seconds and hyphen
          time_str[index++] = sec_str[0];
          time_str[index++] = sec_str[1];
          time_str[index++] = ' ';
          index += 1;

          // Add day and hyphen
          time_str[index++] = day_str[0];
          time_str[index++] = day_str[1];
          time_str[index++] = '-';
          index += 1;

          // Add month and hyphen
          time_str[index++] = month_str[0];
          time_str[index++] = month_str[1];
          time_str[index++] = '-';
          index += 1;

          // Add year
          time_str[index++] = year_str[0];
          time_str[index++] = year_str[1];
          time_str[index++] = year_str[2];
          time_str[index++] = year_str[3];

          // Ensure null termination
          time_str[index] = '\0';

          flanterm_write(ft_ctx, "[RTC] The current date and time is: ", 39);
          flanterm_write(ft_ctx, time_str, 24);
          flanterm_write(ft_ctx, "\n", 2);
          
          serial_puts("\n");
        }
    }
}