#include <stdio.h>
#include <diskfs.h>
#include <stdlib.h>

void main(void)
{
    char name[256];
    char buf[16384];
    uint32_t got = 0;

    if (ShellArgCount() < 1)
    {
        puts("usage: hexdump <file>");
        return;
    }
    ShellArgGet(0, name, (uint32_t)sizeof(name));
    if (!DiskFsRead(name, buf, (uint32_t)sizeof(buf), &got))
    {
        printf("hexdump: %s: read failed\n", name);
        return;
    }
    printf("got=%u\n", got);
    {
        int i;
        for (i = 0; i < 32; i++)
        {
            printf("%02x ", (unsigned int)(unsigned char)buf[i]);
        }
        printf("\n");
    }
}
