#include <stdio.h>
#include <diskfs.h>
#include <stdlib.h>

void main(void)
{
    char src[256];
    char dst[256];
    char buf[16384];
    uint32_t got = 0;

    if (ShellArgCount() < 2)
    {
        puts("usage: cp <src> <dst>");
        return;
    }
    ShellArgGet(0, src, (uint32_t)sizeof(src));
    ShellArgGet(1, dst, (uint32_t)sizeof(dst));
    if (!DiskFsRead(src, buf, (uint32_t)sizeof(buf), &got))
    {
        printf("cp: %s: not found\n", src);
        return;
    }
    if (!DiskFsWrite(dst, buf, got))
    {
        printf("cp: %s: write failed\n", dst);
        return;
    }
    printf("cp: %s -> %s (%d bytes)\n", src, dst, got);
}
