#include <time.h>
#include <stdio.h>
#include <stdlib.h>

void main(void)
{
    time_t t;
    clock_t a, b;

    srand(137);
    time(&t);
    printf("up: %u seconds\n", (unsigned int)t);

    a = clock();
    KernelSleep(200);
    b = clock();
    printf("sleep delta: %u ms\n", (unsigned int)(b - a));

    printf("rand: %d\n", rand() % 100);
}