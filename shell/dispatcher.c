#include "dispatcher.h"
#include "string.h"
#include "bin/cmd.h"
#include "terminal.h"
#include "../panic/panic.h"

void DispatcherDispatch(const PARSED_COMMAND *Command){
    if (strcmp(Command->Command, "regout") == 0 || strcmp(Command->Command, "RegOut") == 0){
        CmdEcho(Command);
        return;
    }
    if (strcmp(Command->Command, "regfill") == 0 || strcmp(Command->Command, "RegFill") == 0){
        CmdFill(Command);
        return;
    }
    if (strcmp(Command->Command, "help") == 0){
        CmdHelp();
        return;
    } 
    if (strcmp(Command->Command, "about") == 0){
        CmdAbout();
        return;
    }
    if (strcmp(Command->Command, "m5sfr0mnu11") == 0){
        CmdMsgFromNull();
        return;
    }
    if (Command->Command[0] == '\0')
    {  
        CmdNewLine();
        return;
    }
    if (strcmp(Command->Command, "clear") == 0){
        CmdPurge();
        return;
    }
    if (strcmp(Command->Command, "panic") == 0){
        PanicKernel("Called Panic API For testing", (uintptr_t)PanicKernel);
        return;
    }
    if (strcmp(Command->Command, "DevKit") == 0 || strcmp(Command->Command, "devkit") == 0){
        CmdDevKit();
        return;
    }
    if (strcmp(Command->Command, "regcreate") == 0){
        if (Command->ArgCount == 0){
            TerminalPrintLine32("Not Enough Arguments, 1 Arg needed", 0x00FF0000);
        }
        CmdRegTo(Command->Args[0]);
        return;
    }
    if (strcmp(Command->Command, "delreg") == 0){
        if (Command->ArgCount == 0){
            TerminalPrintLine32("Not Enough Arguments, 1 Arg needed", 0x00FF0000);
        }
        CmdDelReg(Command->Args[0]);
        return;
    }
    if (strcmp(Command->Command, "fregout") == 0){
        if (Command->ArgCount == 0){
            TerminalPrintLine32("Not Enough Arguments, 1 Arg needed", 0x00FF0000);
        }
        CmdfRegOut(Command->Args[0]);
        return;
    }
    if (strcmp(Command->Command, "regin") == 0)
    {

        if (Command->ArgCount < 2)
        {
            TerminalPrintLine32(
                "Usage: regin <file> <text>",
                0x00FF0000
            );
            return;
        }

        CmdRegIn(
        Command->Args[0],                 
        Command->Args[1],                 
        strlen(Command->Args[1]) + 1       
    );

    return;
    }
    TerminalWrite32("Kernel/shell/bin/cmd.c: ", PromColor);
    TerminalWrite32("Error: ", 0xFFFF0000);
    TerminalWrite32("Command Not found or Corrupted API: ", PromColor);
    TerminalPrintLine32(Command->Command, PromColor);
}