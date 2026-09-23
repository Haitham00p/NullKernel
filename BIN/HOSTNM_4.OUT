#include <stdio.h>
#include <diskfs.h>

void main(void)
{
    char buf[256];
    uint32_t got = 0;

    if (!DiskFsChdirPath("/etc"))
    {
        puts("hostname: /etc missing");
        return;
    }
    if (!DiskFsRead("hostname", buf, (uint32_t)sizeof(buf), &got))
    {
        puts("hostname: /etc/hostname missing");
        DiskFsCdRoot();
        return;
    }
    buf[got] = '\0';
    printf("%s", buf);
    DiskFsCdRoot();
}
