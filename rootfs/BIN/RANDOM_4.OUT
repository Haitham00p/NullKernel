#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <drivers/timer/PIT.h>

static unsigned int rnd;

static unsigned int RndNext(void)
{
    rnd = rnd * 1103515245u + 12345u;
    return rnd;
}

void main(void)
{
    char arg[64];
    unsigned int limit = 100u;
    unsigned int value;

    rnd = (unsigned int)PITGetMilliseconds() ^ 0x85EBCA6Bu;
    if (ShellArgCount() >= 1)
    {
        ShellArgGet(0, arg, (uint32_t)sizeof(arg));
        limit = (unsigned int)atoi(arg);
    }
    if (limit == 0u)
    {
        limit = 100u;
    }
    value = RndNext() % limit;
    printf("%u\n", value);
}
