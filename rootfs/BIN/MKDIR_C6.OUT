#include <stdio.h>
#include <diskfs.h>
#include <stdlib.h>

void main(void)
{
    char name[256];

    if (ShellArgCount() < 1)
    {
        puts("usage: mkdir <dir>");
        return;
    }
    ShellArgGet(0, name, (uint32_t)sizeof(name));
    if (DiskFsMkdir(name))
    {
        printf("mkdir: %s created\n", name);
    }
    else
    {
        printf("mkdir: %s failed\n", name);
    }
}
