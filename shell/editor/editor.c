#include "editor.h"

#include "fs/vfs/vfs.h"
#include "kernel/terminal/terminal.h"
#include "lib/string/string.h"

#define EDITOR_CAPACITY 4096U
#define EDITOR_PATH_CAPACITY 32U

#define EDITOR_BACKGROUND 0x00000000
#define EDITOR_HEADER     0x00224E8AU
#define EDITOR_TEXT       0xFFFFFFFF
#define EDITOR_STATUS     0x0057DB92U
#define EDITOR_ERROR      0x00FF5C77U

static char EditorPath[EDITOR_PATH_CAPACITY];
static char EditorBuffer[EDITOR_CAPACITY];
static uint32_t EditorLength;
static bool EditorRunning;
static bool EditorDirty;

static void EditorRender(void)
{
    TerminalClear32(EDITOR_BACKGROUND);
    TerminalSetCursor32(16, 16);
    TerminalPrintLine32("NULLEDIT  |  Ctrl+S save  |  Ctrl+Q quit", EDITOR_TEXT);
    TerminalPrintLine32("----------------------------------------------------------------", EDITOR_HEADER);
    TerminalWrite32("FILE: ", EDITOR_STATUS);
    TerminalPrintLine32(EditorPath, EDITOR_TEXT);
    TerminalPrintLine32("----------------------------------------------------------------", EDITOR_HEADER);
    TerminalWrite32(EditorBuffer, EDITOR_TEXT);
    TerminalNewLine32(EDITOR_BACKGROUND);
    
}

static void EditorSave(void)
{
    if (VfsWrite(EditorPath, EditorBuffer, EditorLength))
    {
        EditorDirty = false;
    }
}

bool EditorOpen(const char *Path)
{
    const void *Data;
    uint64_t Size;

    if (Path == 0 || Path[0] == '\0' || strlen(Path) >= EDITOR_PATH_CAPACITY)
    {
        return false;
    }

    strcpy(EditorPath, Path);
    EditorLength = 0;
    EditorBuffer[0] = '\0';
    EditorDirty = false;

    if (VfsRead(Path, &Data, &Size))
    {
        if (Size >= EDITOR_CAPACITY) Size = EDITOR_CAPACITY - 1U;
        if (Size != 0) memcpy(EditorBuffer, Data, Size);
        EditorLength = (uint32_t)Size;
        EditorBuffer[EditorLength] = '\0';
    }
    else if (!VfsCreate(Path))
    {
        return false;
    }

    EditorRunning = true;
    EditorRender();
    return true;
}

bool EditorIsActive(void)
{
    return EditorRunning;
}

void EditorHandleKey(char Character, bool ControlHeld)
{
    if (!EditorRunning) return;

    if (ControlHeld && (Character == 's' || Character == 'S'))
    {
        EditorSave();
        EditorRender();
        return;
    }
    if (ControlHeld && (Character == 'q' || Character == 'Q'))
    {
        EditorRunning = false;
        TerminalClear32(EDITOR_BACKGROUND);
        TerminalPrintLine32("NullEdit closed.", EDITOR_STATUS);
        return;
    }
    if (Character == '\b')
    {
        if (EditorLength != 0)
        {
            EditorBuffer[--EditorLength] = '\0';
            EditorDirty = true;
            EditorRender();
        }
        return;
    }
    if ((Character == '\n' || (Character >= ' ' && Character <= '~')) &&
        EditorLength < EDITOR_CAPACITY - 1U)
    {
        EditorBuffer[EditorLength++] = Character;
        EditorBuffer[EditorLength] = '\0';
        EditorDirty = true;
        EditorRender();
    }
}
