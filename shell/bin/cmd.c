#include "cmd.h"
#include "terminal.h"
#include "../parser.h"
#include <stdint.h>
#include <stddef.h>
#include "../../string/string.h"
#include "../shell.h"
#include "../../ramfs/ramfs.h"
#include <stdbool.h>

void CmdHelp(){
    TerminalPrintLine32(" help : Show All Available Commands", PromColor);
    TerminalPrintLine32(" about : About this OS", PromColor);
    TerminalPrintLine32(" clear : Clear History", PromColor);
    TerminalPrintLine32(" regout : (Regester Out) To print out, (STR-ONLY)", PromColor);
    TerminalPrintLine32(" regfill : (Register Fill) To Change Colors of The Shell", PromColor);
    TerminalPrintLine32(" devkit : Show Development Kits", PromColor);
    TerminalPrintLine32(" regin : Write Registry On a Existing File", PromColor);
    TerminalPrintLine32(" regcreate : Create a Registry", PromColor);
    TerminalPrintLine32(" fregout : To print data on existing file", PromColor);
    TerminalPrintLine32(" delreg : To delete a rigistry", PromColor);
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
void CmdFill(PARSED_COMMAND *Command)
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

void CmdEcho(PARSED_COMMAND *command)
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