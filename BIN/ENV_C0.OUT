#include <stdio.h>
#include <diskfs.h>

void main(void)
{
    char path[256];

    DiskFsPwd(path, (uint32_t)sizeof(path));
    printf("PWD     = %s\n", path);
    puts("PATH    = /bin:.");
    puts("HOME    = /home/Root");
    puts("SHELL   = /bin/sh");
    puts("LANG    = C");
    puts("TERM    = 1280x800");
}
