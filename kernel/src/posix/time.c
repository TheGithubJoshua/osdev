#include "time.h"
#include "../drivers/cmos/rtc.h"

static const int mdays[12] = {
    31,28,31,30,31,30,31,31,30,31,30,31
};

int gettimeofday(struct timeval *restrict tp, void *restrict tzp) {
	rtc_t r = read_rtc();

	int year  = r.century * 100 + r.year;
	int month = r.month - 1;  // 0-based
	int day   = r.day - 1;    // days since month start

	uint64_t days = 0;

	/* years */
	for (int y = 1970; y < year; y++)
	    days += IS_LEAP(y) ? 366 : 365;

	/* months */
	for (int m = 0; m < month; m++) {
	    days += mdays[m];
	    if (m == 1 && IS_LEAP(year))
	        days++;
	}

	days += day;

	tp->tv_sec = days * 86400ULL
	     + r.hour   * 3600ULL
	     + r.minute * 60ULL
	     + r.second;

	tp->tv_usec = tp->tv_sec*1000000;
	
	return 0;
}