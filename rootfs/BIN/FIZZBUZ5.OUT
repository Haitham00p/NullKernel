#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

void main(void)
{
    char arg[64];
    int n = 100;
    int i;

    if (ShellArgCount() >= 1)
    {
        ShellArgGet(0, arg, (uint32_t)sizeof(arg));
        n = atoi(arg);
    }
    if (n < 1)
    {
        n = 100;
    }
    for (i = 1; i <= n; i++)
    {
        if (i % 15 == 0)
        {
            puts("FizzBuzz");
        }
        else if (i % 3 == 0)
        {
            puts("Fizz");
        }
        else if (i % 5 == 0)
        {
            puts("Buzz");
        }
        else
        {
            printf("%d\n", i);
        }
    }
}
