#include <stdio.h>

void main(void)
{
    int i;

    for (i = 32; i < 127; i++)
    {
        if ((i - 32) % 16 == 0)
        {
            printf("%03d: ", i);
        }
        printf("%c ", i);
        if ((i - 32) % 16 == 15)
        {
            putchar('\n');
        }
    }
    putchar('\n');
}
