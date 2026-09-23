#include <stdio.h>
#include <diskfs.h>

void main(void)
{
    char path[256];
    char list[1024];
    uint32_t count;

    puts("         .---.");
    puts("        /     \\      NullOS - self hosted");
    puts("        | () () |    64-bit x86, ncc compiled");
    puts("         \\  ^  /     commands are .c sources");
    puts("          `---'");
    printf("      NullOS 1.0\n");
    DiskFsPwd(path, (uint32_t)sizeof(path));
    printf("      cwd: %s\n", path);
    count = DiskFsListNames(list, (uint32_t)sizeof(list));
    printf("      rootfs entries: %d\n", count);
}
