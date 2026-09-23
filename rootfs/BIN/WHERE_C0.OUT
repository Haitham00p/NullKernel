#include <stdio.h>
#include <diskfs.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* where: print every path where a file name appears in the tree.
   Usage: where <name> [<start-dir>]   (name match is case-insensitive) */

static char Name[128];
static char Path[512];
static uint32_t Matches = 0;

static bool PartialMatch(const char *Hay, const char *Needle)
{
    uint32_t i;
    uint32_t j;

    for (i = 0; Hay[i] != '\0'; i++)
    {
        j = 0;
        while (Needle[j] != '\0')
        {
            if (tolower_c(Hay[i + j]) != tolower_c(Needle[j]))
            {
                break;
            }
            j = j + 1U;
        }
        if (Needle[j] == '\0')
        {
            return true;
        }
    }
    return false;
}

static void LocateRecurse(void)
{
    char list[2048];
    char line[256];
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t PathLen;

    DiskFsListNames(list, (uint32_t)sizeof(list));
    while (list[i] != '\0')
    {
        j = 0;
        while (list[i] != '\0' && list[i] != '\n' &&
               j < (uint32_t)sizeof(line) - 1U)
        {
            line[j] = list[i];
            j = j + 1U;
            i = i + 1U;
        }
        line[j] = '\0';
        if (list[i] == '\n')
        {
            i = i + 1U;
        }
        if (j == 0U)
        {
            continue;
        }
        if (strcmp(line, ".") == 0 || strcmp(line, "..") == 0)
        {
            continue;
        }
        if (PartialMatch(line, Name))
        {
            if (strcmp(Path, "/") == 0)
            {
                printf("/%s\n", line);
            }
            else
            {
                printf("%s/%s\n", Path, line);
            }
            Matches = Matches + 1U;
        }
        if (DiskFsIsDirectory(line))
        {
            if (DiskFsCd(line))
            {
                PathLen = strlen(Path);
                if (PathLen > 1U)
                {
                    Path[PathLen] = '/';
                    Path[PathLen + 1U] = '\0';
                    strcpy(Path + PathLen + 1U, line);
                }
                else
                {
                    strcpy(Path + 1U, line);
                }
                LocateRecurse();
                if (PathLen > 1U)
                {
                    Path[PathLen] = '\0';
                }
                else
                {
                    Path[1] = '\0';
                }
                DiskFsCd("..");
            }
        }
    }
}

void main(void)
{
    char arg[512];

    if (ShellArgCount() < 1U)
    {
        puts("usage: where <name> [<start-dir>]");
        return;
    }
    ShellArgGet(0, Name, (uint32_t)sizeof(Name));
    DiskFsCdRoot();
    if (ShellArgCount() >= 2U)
    {
        ShellArgGet(1, arg, (uint32_t)sizeof(arg));
        if (!DiskFsCd(arg))
        {
            printf("where: %s: no such directory\n", arg);
            return;
        }
    }
    Path[0] = '/';
    Path[1] = '\0';
    LocateRecurse();
    if (Matches == 0U)
    {
        printf("where: no match for \"%s\"\n", Name);
    }
}