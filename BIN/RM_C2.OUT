#include <stdio.h>
#include <diskfs.h>
#include <stdlib.h>

void main(void)
{
    char name[256];

    if (ShellArgCount() < 1)
    {
        puts("usage: rm <file>");
        return;
    }
    ShellArgGet(0, name, (uint32_t)sizeof(name));
    if (DiskFsRemove(name))
    {
        printf("rm: %s removed\n", name);
    }
    else
    {
        printf("rm: %s: not found\n", name);
    }
}
