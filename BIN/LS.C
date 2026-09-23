#include <stdio.h>
#include <diskfs.h>
#include <stdint.h>
#include <terminal.h>
#include <stdlib.h>

#define LS_COLOR_DIR   0x0040A0FFu
#define LS_COLOR_EXE   0x0037D67Au
#define LS_COLOR_FILE  0x00FFFFFFu

static uint32_t LsColorFor(const char *Name)
{
    uint32_t Len = 0;

    while (Name[Len] != '\0')
    {
        Len++;
    }
    if (DiskFsIsDirectory(Name))
    {
        return LS_COLOR_DIR;
    }
    if (Len < 4U || Name[Len - 4U] != '.')
    {
        return LS_COLOR_FILE;
    }
    if ((Name[Len - 3U] == 'E' || Name[Len - 3U] == 'e') &&
        (Name[Len - 2U] == 'X' || Name[Len - 2U] == 'x') &&
        (Name[Len - 1U] == 'E' || Name[Len - 1U] == 'e'))
    {
        return LS_COLOR_EXE;
    }
    if ((Name[Len - 3U] == 'E' || Name[Len - 3U] == 'e') &&
        (Name[Len - 2U] == 'L' || Name[Len - 2U] == 'l') &&
        (Name[Len - 1U] == 'F' || Name[Len - 1U] == 'f'))
    {
        return LS_COLOR_EXE;
    }
    return LS_COLOR_FILE;
}

void main(void)
{
    char list[2048];
    char line[256];
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t col = 0;
    uint32_t len = 0;
    term_screen_t *screen = TerminalGetScreen();
    uint32_t pane_cols = (TerminalIsSplit() && screen != NULL)
        ? screen->cols / 2U
        : (screen != NULL ? screen->cols : 80U);
    if (pane_cols < 1U) pane_cols = 1U;

    /* Optional path argument: ls [path] */
    char path[128];
    uint32_t have_path = 0;
    char saved[128];
    uint32_t changed = 0;

    if (ShellArgCount() > 0U)
    {
        if (ShellArgGet(0U, path, (uint32_t)sizeof(path)))
        {
            have_path = 1;
        }
    }

    if (have_path)
    {
        DiskFsPwd(saved, (uint32_t)sizeof(saved));
        if (!DiskFsChdirPath(path))
        {
            TerminalWrite32("ls: cannot access ", 0x00FF5555u);
            TerminalWrite32(path, 0x00FF5555u);
            TerminalPutChar32('\n', 0x00FF5555u);
            return;
        }
        changed = 1;
    }

    DiskFsListNames(list, (uint32_t)sizeof(list));

    while (list[i] != '\0')
    {
        if (list[i] == '\n')
        {
            line[j] = '\0';
            if (j > 0U)
            {
                len = 0;
                while (line[len] != '\0')
                {
                    len++;
                }
                if (col + len >= pane_cols)
                {
                    TerminalPutChar32('\n', LS_COLOR_FILE);
                    col = 0U;
                }
                TerminalWrite32(line, LsColorFor(line));
                col = col + len + 4U;
                TerminalPutChar32(' ', LS_COLOR_FILE);
                TerminalPutChar32(' ', LS_COLOR_FILE);
                TerminalPutChar32(' ', LS_COLOR_FILE);
                TerminalPutChar32(' ', LS_COLOR_FILE);
            }
            j = 0;
        }
        else if (j < (uint32_t)sizeof(line) - 1U)
        {
            line[j++] = list[i];
        }
        i++;
    }
    if (col != 0U)
    {
        TerminalPutChar32('\n', LS_COLOR_FILE);
    }

    if (changed)
    {
        DiskFsChdirPath(saved);
    }
}
