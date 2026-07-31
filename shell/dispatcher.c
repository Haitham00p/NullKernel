#include "dispatcher.h"
#include "lib/string/string.h"
#include "commands/cmd.h"
#include "kernel/terminal/terminal.h"
#include "kernel/panic/panic.h"

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
    if (strcmp(Command->Command, "ls") == 0) {
        if (Command->ArgCount != 0U) { TerminalPrintLine32("Usage: ls", 0x00FF0000); return; }
        CmdLs(); return;
    }
    if (strcmp(Command->Command, "touch") == 0) {
        if (Command->ArgCount != 1U) { TerminalPrintLine32("Usage: touch <file>", 0x00FF0000); return; }
        CmdTouch(Command->Args[0]); return;
    }
    if (strcmp(Command->Command, "cat") == 0) {
        if (Command->ArgCount != 1U) { TerminalPrintLine32("Usage: cat <file>", 0x00FF0000); return; }
        CmdCat(Command->Args[0]); return;
    }
    if (strcmp(Command->Command, "rm") == 0) {
        if (Command->ArgCount != 1U) { TerminalPrintLine32("Usage: rm <file>", 0x00FF0000); return; }
        CmdRm(Command->Args[0]); return;
    }
    if (strcmp(Command->Command, "write") == 0) { CmdWrite(Command); return; }
    if (strcmp(Command->Command, "suf") == 0) {
        if (Command->ArgCount != 1U) { TerminalPrintLine32("Usage: suf <command>", 0x00FF0000); return; }
        CmdSuf(Command->Args[0]); return;
    }
    if (strcmp(Command->Command, "edit") == 0) {
        if (Command->ArgCount != 1U) { TerminalPrintLine32("Usage: edit <file>", 0x00FF0000); return; }
        CmdEdit(Command->Args[0]); return;
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
            return;
        }
        CmdRegTo(Command->Args[0]);
        return;
    }
    if (strcmp(Command->Command, "delreg") == 0){
        if (Command->ArgCount == 0){
            TerminalPrintLine32("Not Enough Arguments, 1 Arg needed", 0x00FF0000);
            return;
        }
        CmdDelReg(Command->Args[0]);
        return;
    }
    if (strcmp(Command->Command, "fregout") == 0){
        if (Command->ArgCount == 0){
            TerminalPrintLine32("Not Enough Arguments, 1 Arg needed", 0x00FF0000);
            return;
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
