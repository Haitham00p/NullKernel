#ifndef LIBC_TIME_H
#define LIBC_TIME_H

#include <stdint.h>

/* The PIT drives the system clock at 1000 Hz. */
#define CLOCKS_PER_SEC 1000U

typedef uint32_t clock_t;
typedef uint64_t time_t;

/* Milliseconds elapsed since boot. */
clock_t clock(void);

/* Seconds elapsed since boot; also stored through Tloc when non-NULL. */
time_t time(time_t *Tloc);

/* Suspends the current program for Milliseconds. */
void KernelSleep(uint64_t Milliseconds);

#endif