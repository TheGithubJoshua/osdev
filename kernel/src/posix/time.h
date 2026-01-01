#pragma once
#include <stdint.h>

#define IS_LEAP(y) (((y) % 4 == 0) && (((y) % 100 != 0) || ((y) % 400 == 0))) // is a given year a leap year

typedef long suseconds_t;
typedef long time_t;

typedef struct timeval {
	time_t tv_sec; // seconds since epoch
	suseconds_t tv_usec; // microseconds since epoch
} timeval;

int gettimeofday(struct timeval *restrict tp, void *restrict tzp);