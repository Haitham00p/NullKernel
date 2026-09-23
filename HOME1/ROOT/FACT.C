#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

static unsigned long Fact(unsigned long n)
{
    if (n <= 1)
    {
        return 1;
    }
    return n * Fact(n - 1);
}

void main(void)
{
    char arg[64];
    unsigned long n = 10;

    if (ShellArgCount() >= 1)
    {
        ShellArgGet(0, arg, (uint32_t)sizeof(arg));
        n = strtoul(arg, 0, 10);
    }
    printf("%lu! = %lu\n", n, Fact(n));
}
