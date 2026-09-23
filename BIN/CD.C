#include <stdio.h>
#include <diskfs.h>
#include <stdlib.h>

void main(void)
{
    char path[256];

    if (ShellArgCount() < 1)
    {
        DiskFsCdRoot();
        puts("cd: /");
        return;
    }

    ShellArgGet(0, path, (uint32_t)sizeof(path));

    if (DiskFsChdirPath(path))
    {
        printf("cd: %s\n", path);
    }
    else
    {
        printf("cd: %s: no such directory\n", path);
    }
}
