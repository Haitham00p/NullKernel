#include <stdio.h>
#include <stdlib.h>

void main(void)
{
    uint32_t i;

    for (i = 0; i < ShellArgCount(); i++)
    {
        char arg[256];

        ShellArgGet(i, arg, (uint32_t)sizeof(arg));
        if (i > 0)
        {
            putchar(' ');
        }
        printf("%s", arg);
    }
    putchar('\n');
}
