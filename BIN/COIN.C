#include <stdio.h>
#include <drivers/timer/PIT.h>

void main(void)
{
    uint64_t ms = PITGetMilliseconds();

    if ((ms & 1U) == 0U)
    {
        puts("heads");
    }
    else
    {
        puts("tails");
    }
}
