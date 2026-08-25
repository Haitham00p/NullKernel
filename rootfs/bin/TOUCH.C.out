#include <stdio.h>
#include <diskfs.h>
#include <stdlib.h>

void main(void)
{
    char name[256];

    if (ShellArgCount() < 1)
    {
        puts("usage: touch <file>");
        return;
    }
    ShellArgGet(0, name, (uint32_t)sizeof(name));
    if (DiskFsTouch(name))
    {
        printf("touch: %s\n", name);
    }
    else
    {
        printf("touch: %s failed\n", name);
    }
}
