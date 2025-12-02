// https://wiki.osdev.org/CMOS
#include "rtc.h"
#include "../../iodebug.h"
#include "../../acpi/acpi.h"

#define CURRENT_YEAR        2025                            // change each year

int century_register = 0x00;

static inline void set_century() {                                
      fadt_t *fadt = get_table("FACP", 69);
      century_register = fadt->Century;
}

enum {
      cmos_address = 0x70,
      cmos_data    = 0x71
};

int get_update_in_progress_flag() {
      outb(cmos_address, 0x0A);
      return (inb(cmos_data) & 0x80);
}

unsigned char get_RTC_register(int reg) {
      outb(cmos_address, reg);
      return inb(cmos_data);
}

rtc_t read_rtc() {
      rtc_t rtc;
      set_century();

      // Note: This uses the "read registers until you get the same values twice in a row" technique
      //       to avoid getting dodgy/inconsistent values due to RTC updates

      while (get_update_in_progress_flag());                // Make sure an update isn't in progress
      rtc.second = get_RTC_register(0x00);
      rtc.minute = get_RTC_register(0x02);
      rtc.hour = get_RTC_register(0x04);
      rtc.day = get_RTC_register(0x07);
      rtc.month = get_RTC_register(0x08);
      rtc.year = get_RTC_register(0x09);
      if(century_register != 0) {
            rtc.century = get_RTC_register(century_register);
      }

      do {
            rtc.last_second = rtc.second;
            rtc.last_minute = rtc.minute;
            rtc.last_hour = rtc.hour;
            rtc.last_day = rtc.day;
            rtc.last_month = rtc.month;
            rtc.last_year = rtc.year;
            rtc.last_century = rtc.century;

            while (get_update_in_progress_flag());           // Make sure an update isn't in progress
            rtc.second = get_RTC_register(0x00);
            rtc.minute = get_RTC_register(0x02);
            rtc.hour = get_RTC_register(0x04);
            rtc.day = get_RTC_register(0x07);
            rtc.month = get_RTC_register(0x08);
            rtc.year = get_RTC_register(0x09);
            if(century_register != 0) {
                  rtc.century = get_RTC_register(century_register);
            }
      } while( (rtc.last_second != rtc.second) || (rtc.last_minute != rtc.minute) || (rtc.last_hour != rtc.hour) ||
               (rtc.last_day != rtc.day) || (rtc.last_month != rtc.month) || (rtc.last_year != rtc.year) ||
               (rtc.last_century != rtc.century) );

      rtc.registerB = get_RTC_register(0x0B);

      // Convert BCD to binary values if necessary

      if (!(rtc.registerB & 0x04)) {
            rtc.second = (rtc.second & 0x0F) + ((rtc.second / 16) * 10);
            rtc.minute = (rtc.minute & 0x0F) + ((rtc.minute / 16) * 10);
            rtc.hour = ( (rtc.hour & 0x0F) + (((rtc.hour & 0x70) / 16) * 10) ) | (rtc.hour & 0x80);
            rtc.day = (rtc.day & 0x0F) + ((rtc.day / 16) * 10);
            rtc.month = (rtc.month & 0x0F) + ((rtc.month / 16) * 10);
            rtc.year = (rtc.year & 0x0F) + ((rtc.year / 16) * 10);
            if(century_register != 0) {
                  rtc.century = (rtc.century & 0x0F) + ((rtc.century / 16) * 10);
            }
      }

      // Convert 12 hour clock to 24 hour clock if necessary

      if (!(rtc.registerB & 0x02) && (rtc.hour & 0x80)) {
            rtc.hour = ((rtc.hour & 0x7F) + 12) % 24;
      }

      // Calculate the full (4-digit) year

      if(century_register != 0) {
            //rtc.year += rtc.century * 100;
            // ignore it anyway because it's unreliable as fuck :)
      } else {
            rtc.year += (CURRENT_YEAR / 100) * 100;
            if(rtc.year < CURRENT_YEAR) rtc.year += 100;
      }

      return rtc;
}

// helper function to get day of year
int day_of_year(int day, int month, int year)
{
    static const int days_before_month[12] =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    int doy = days_before_month[month - 1] + day;

    // leap year adjustment
    int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));

    if (leap && month > 2)
        doy += 1;

    return doy;
}
