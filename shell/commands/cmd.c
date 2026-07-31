#include "cmd.h"
#include "kernel/terminal/terminal.h"
#include "../parser.h"
#include <stdint.h>
#include <stddef.h>
#include "lib/string/string.h"
#include "../shell.h"
#include "fs/ramfs/ramfs.h"
#include "fs/vfs/vfs.h"
#include "shell/editor/editor.h"
#include <stdbool.h>

void CmdHelp(){
    TerminalPrintLine32("NULLOS SHELL COMMAND REFERENCE", 0x005AA9FF);
    TerminalPrintLine32("------------------------------------------------", 0x00314A70);
    TerminalPrintLine32("SYSTEM", 0x0057DB92);
    TerminalPrintLine32("  help                         Show this command reference", PromColor);
    TerminalPrintLine32("  about                        Show OS and kernel information", PromColor);
    TerminalPrintLine32("  clear                        Clear the terminal screen", PromColor);
    TerminalPrintLine32("  panic                        Trigger a test kernel panic", PromColor);
    TerminalPrintLine32("  devkit                       Show developer diagnostics", PromColor);
    TerminalPrintLine32("  suf <command>                Show command source dependencies", PromColor);
    TerminalPrintLine32("FILESYSTEM / VFS", 0x0057DB92);
    TerminalPrintLine32("  ls                           List VFS files", PromColor);
    TerminalPrintLine32("  touch <file>                 Create an empty VFS file", PromColor);
    TerminalPrintLine32("  write <file> <text>          Write text to a VFS file", PromColor);
    TerminalPrintLine32("  cat <file>                   Print a VFS file", PromColor);
    TerminalPrintLine32("  rm <file>                    Remove a VFS file", PromColor);
    TerminalPrintLine32("  edit <file>                  Open NullEdit (Ctrl+S/Ctrl+Q)", PromColor);
    TerminalPrintLine32("LEGACY REGISTRY API", 0x0057DB92);
    TerminalPrintLine32("  regout <text> <color>        Print colored text (alias: RegOut)", PromColor);
    TerminalPrintLine32("  regfill <fg> <bg>            Set shell colors (alias: RegFill)", PromColor);
    TerminalPrintLine32("  regcreate <file>             Create a registry file", PromColor);
    TerminalPrintLine32("  regin <file> <text>          Write registry text", PromColor);
    TerminalPrintLine32("  fregout <file>               Print registry contents", PromColor);
    TerminalPrintLine32("  delreg <file>                Delete a registry file", PromColor);
    TerminalPrintLine32("------------------------------------------------", 0x00314A70);
    TerminalPrintLine32("Paths currently use the in-memory VFS backend.", 0x009AA8BD);
}

void CmdDevKit(){
    TerminalPrintLine32(" panic : For Panic The kernel", 0xFFFF0000);
}

void CmdMsgFromNull(){
    TerminalPrintLine32("Welcome to NullOS 0.1, my first OS ever!", 0x00FF00FF);
    TerminalPrintLine32("NullOS created by Null, All right reserved", 0x00FF00FF);
    TerminalPrintLine32("Created with hard work, hope, and countless sleepless nights!", 0x00FF00FF);
    TerminalPrintLine32("Please wait a little while longer,", 0x00FF00FF);
    TerminalPrintLine32("Because one day, i'll find my phone 99+ unanswered calls from little Billy", 0x00FF00FF);
}

void CmdAbout(){
    TerminalWrite32("Version: ", 0xFFFFFFFF);
    TerminalPrintLine32("NullOS 0.1", 0x00FF00FF);
    TerminalWrite32("Kernel: ", 0xFFFFFFFF);
    TerminalPrintLine32("Null Kernel", 0x00FF00FF);
    TerminalWrite32("Architecture: ", 0xFFFFFFFF);
    TerminalPrintLine32("x86_64", 0x00FF00FF);
    TerminalWrite32("Bootloader: ", 0xFFFFFFFF);
    TerminalPrintLine32("Limine", 0x00FF00FF);
}
void CmdFill(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount != 2)
    {
        TerminalPrintLine32(
            "Usage: regfill [FG] [BG]",
            0xFFFFFFFF
        );
        return;
    }

    uint32_t ColorFG = 0xFFFFFFFF;
    uint32_t ColorBG = 0x00000000;

    if (!StringHexToUInt32(Command->Args[0], &ColorFG))
    {
        
        TerminalWrite32(
            "Invalid Foreground Color: ",
            0x00FF0000
        );
        TerminalPrintLine32("Required ARGB-Hex Color", PromColor);
        return;
    }

    if (!StringHexToUInt32(Command->Args[1], &ColorBG))
    {
        TerminalPrintLine32(
            "Invalid Background Color: ",
            0x00FF0000
        );
        TerminalPrintLine32("Required ARGB-Hex Color", PromColor);
        return;
    }



    TerminalClear32(ColorBG);

    PromColor = ColorFG;
    BackSpaceColor = ColorBG;
}

void CmdNewLine(){
    /* It is Null Needed API, DON'T DELETE THIS. */
}

void CmdPurge(){
    TerminalClear32(BackSpaceColor);
}

void CmdEcho(const PARSED_COMMAND *command)
{
    if (command->ArgCount < 2)
    {
        TerminalWrite32("Kernel/shell/bin/cmd.c: ", PromColor);
        TerminalWrite32("Error: ", 0x00FF0000);
        TerminalPrintLine32(
            "Usage: RegOut [TEXT] [COLOR]",
            PromColor
        );
        return;
    }

    uint32_t ColorIndex = command->ArgCount - 1;
    uint32_t Color;

    if (!StringHexToUInt32(command->Args[ColorIndex], &Color))
    {
        TerminalPrintLine32("Invalid Color", 0x00FF0000);
        return;
    }

    uint32_t TextArgs = command->ArgCount - 1;

    for (uint32_t i = 0; i < TextArgs; i++)
    {
        TerminalWrite32(command->Args[i], Color);

        if (i < TextArgs - 1)
            TerminalWrite32(" ", Color);
    }

    TerminalPutChar32('\n', Color);
}
void CmdfRegOut(const char *Name)
{
    RamFile *File = RamFsFind(Name);

    if (File == NULL)
    {
        TerminalPrintLine32("File Not Found", PromColor);
        return;
    }

    TerminalPrintLine32(File->Data, PromColor);
}

void CmdRegTo(const char *Name){
    RamFsCreate(Name);
}

void CmdRegIn(const char *Name, const void *Data, uint64_t Size)
{
    RamFile *File = RamFsFind(Name);

    if (File == NULL)
    {
        File = RamFsCreate(Name);
    }

    if (File == NULL)
    {
        TerminalPrintLine32("Cannot Create File", PromColor);
        return;
    }

    RamFsWrite(File, Data, Size);
}

void CmdDelReg(const char *Name)
{
    RamFile *File = RamFsFind(Name);

    if (File == NULL)
    {
        TerminalPrintLine32("File Not Found", PromColor);
        return;
    }

    RamFsDelete(File);
}

static bool CmdListVisitor(const VfsFileInfo *File, void *Context)
{
    (void)Context;
    TerminalWrite32(File->Name, PromColor);
    TerminalWrite32("  ", PromColor);
    TerminalPrintHex64Compact(File->Size);
    TerminalPutChar32('\n', PromColor);
    return true;
}

void CmdLs(void)
{
    VfsList(CmdListVisitor, 0);
}

void CmdTouch(const char *Path)
{
    if (!VfsCreate(Path)) TerminalPrintLine32("touch: cannot create file", 0x00FF0000);
}

void CmdCat(const char *Path)
{
    const void *Data;
    uint64_t Size;
    if (!VfsRead(Path, &Data, &Size))
    {
        TerminalPrintLine32("cat: file not found", 0x00FF0000);
        return;
    }
    if (Size != 0) TerminalWrite32((const char *)Data, PromColor);
    TerminalPutChar32('\n', PromColor);
}

void CmdRm(const char *Path)
{
    if (!VfsRemove(Path)) TerminalPrintLine32("rm: file not found", 0x00FF0000);
}

void CmdWrite(const PARSED_COMMAND *Command)
{
    char Text[PARSER_MAX_ARGS * PARSER_MAX_ARG_LENGTH];
    uint32_t Position = 0;

    if (Command->ArgCount < 2U)
    {
        TerminalPrintLine32("Usage: write <file> <text>", 0x00FF0000);
        return;
    }
    for (uint32_t Argument = 1; Argument < Command->ArgCount; Argument++)
    {
        const char *Part = Command->Args[Argument];
        for (uint32_t Index = 0; Part[Index] != '\0' &&
             Position < sizeof(Text) - 1U; Index++) Text[Position++] = Part[Index];
        if (Argument + 1U < Command->ArgCount && Position < sizeof(Text) - 1U)
            Text[Position++] = ' ';
    }
    Text[Position] = '\0';
    if (!VfsWrite(Command->Args[0], Text, Position))
        TerminalPrintLine32("write: cannot write file", 0x00FF0000);
}

static void CmdSufLine(const char *Line)
{
    TerminalPrintLine32(Line, 0x00E8F0FF);
}

void CmdSuf(const char *CommandName)
{
    if (CommandName == 0 || CommandName[0] == '\0')
    {
        TerminalPrintLine32("Usage: suf <command>", 0x00FF5C77);
        return;
    }

    TerminalPrintLine32("NULLOS SOURCE USAGE FINDER", 0x005AA9FF);
    TerminalPrintLine32("------------------------------------------------", 0x00314A70);
    TerminalWrite32("COMMAND: ", 0x0057DB92);
    TerminalPrintLine32(CommandName, 0x00E8F0FF);

    if (strcmp(CommandName, "clear") == 0)
    {
        CmdSufLine("shell/dispatcher.c       -> routes clear to CmdPurge()");
        CmdSufLine("shell/commands/cmd.c     -> CmdPurge() calls TerminalClear32()");
        CmdSufLine("shell/commands/cmd.h     -> command API declaration");
        CmdSufLine("kernel/terminal/terminal.h -> TerminalClear32() declaration");
        CmdSufLine("kernel/terminal/terminal.c -> resets cursor and calls FbCleanScreen32()");
        CmdSufLine("drivers/video_framebuffer/framebuffer.h -> framebuffer API declaration");
        CmdSufLine("drivers/video_framebuffer/framebuffer.c -> clears every framebuffer pixel");
        return;
    }

    if (strcmp(CommandName, "edit") == 0)
    {
        CmdSufLine("shell/dispatcher.c       -> routes edit to CmdEdit()");
        CmdSufLine("shell/commands/cmd.c     -> opens editor mode");
        CmdSufLine("shell/editor/editor.c    -> keyboard editing, rendering, save/quit");
        CmdSufLine("shell/shell.c            -> forwards keyboard input while editing");
        CmdSufLine("fs/vfs/vfs.c             -> reads and writes editor files");
        CmdSufLine("fs/ramfs/ramfs.c          -> current VFS storage backend");
        return;
    }

    if (strcmp(CommandName, "ls") == 0 || strcmp(CommandName, "cat") == 0 ||
        strcmp(CommandName, "touch") == 0 || strcmp(CommandName, "rm") == 0 ||
        strcmp(CommandName, "write") == 0)
    {
        CmdSufLine("shell/dispatcher.c       -> command dispatch");
        CmdSufLine("shell/commands/cmd.c     -> command implementation");
        CmdSufLine("shell/commands/cmd.h     -> command API declaration");
        CmdSufLine("fs/vfs/vfs.c             -> VFS request routing");
        CmdSufLine("fs/vfs/vfs.h             -> VFS API declaration");
        CmdSufLine("fs/ramfs/ramfs.c          -> current in-memory storage backend");
        return;
    }

    TerminalPrintLine32("suf: command is not in the source usage catalog", 0x00FF5C77);
}

void CmdEdit(const char *Path)
{
    if (!EditorOpen(Path))
    {
        TerminalPrintLine32("edit: cannot open file", 0x00FF5C77);
    }
}
