#include <stdio.h>
#include <drivers/timer/PIT.h>

void main(void)
{
    unsigned int picks[6];
    unsigned int i;
    unsigned int j;
    unsigned int draw;
    unsigned int ms;

    ms = (unsigned int)PITGetMilliseconds();
    for (i = 0; i < 6; i++)
    {
        ms = ms * 1103515245u + 12345u;
        draw = ((ms >> 16) % 49u) + 1u;
        for (j = 0; j < i; j++)
        {
            if (picks[j] == draw)
            {
                draw = draw % 49u + 1u;
                j = 0;
            }
        }
        picks[i] = draw;
    }
    printf("lotto numbers:");
    for (i = 0; i < 6; i++)
    {
        printf(" %u", picks[i]);
    }
    putchar('\n');
}
