#include <stdio.h>
#include <drivers/timer/PIT.h>

void main(void)
{
    uint64_t ms = PITGetMilliseconds();
    uint64_t ticks = PITGetTicks();

    printf("uptime: %llu ms\n", ms);
    printf("ticks:  %llu\n", ticks);
}
