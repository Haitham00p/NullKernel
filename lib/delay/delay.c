#include "delay.h"

void SleepMs(uint64_t ms)
{
    while (ms--)
    {
        for (volatile uint64_t i = 0; i < 1000000; i++)
        {
            __asm__ volatile("nop");
        }
    }
}