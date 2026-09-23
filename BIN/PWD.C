#include <stdio.h>
#include <diskfs.h>

void main(void)
{
    char path[256];

    DiskFsPwd(path, (uint32_t)sizeof(path));
    printf("%s\n", path);
}
