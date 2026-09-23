#include "time.h"
#include "../../drivers/timer/PIT.h"
#include "../../libc/time.h"
#include <stddef.h>

void KernelSleep(uint64_t Milliseconds){
    PITSleepMS(Milliseconds);
}

clock_t clock(void)
{
    return (clock_t)(PITGetMilliseconds() & UINT32_MAX);
}

time_t time(time_t *Tloc)
{
    time_t Seconds = (time_t)(PITGetMilliseconds() / 1000U);

    if (Tloc != NULL)
    {
        *Tloc = Seconds;
    }
    return Seconds;
}