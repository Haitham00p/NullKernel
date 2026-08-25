#include "dispatcher.h"
#include "lib/string/string.h"
#include "commands/cmd.h"
#include "kernel/terminal/terminal.h"
#include "kernel/panic/panic.h"
#include "../fs/ISO9660/iso9660.h"
#include "../include/kshell.h"
#include "libc/stdlib.h"

void DispatcherDispatch(const PARSED_COMMAND *Command){
    /* Disk commands shadow the built-ins: when /BIN/<cmd>.EXE exists on
       the installed volume, it runs instead of the kernel built-in. */
    if (DispatcherTryDiskCommand(Command)) return;

    if (strcmp(Command->Command, "regfill") == 0 || strcmp(Command->Command, "RegFill") == 0){
        CmdFill(Command);
        return;
    }
    if (strcmp(Command->Command, "help") == 0){
        if (Command->ArgCount == 0U) { CmdHelp(0); return; }
        if (Command->ArgCount == 1U) { CmdHelp(Command->Args[0]); return; }
        TerminalPrintLine32("Usage: help [category]", 0x00FF0000);
        return;
    } 
    /* ls, cd, pwd, mkdir, rmdir, touch, cat, rm are provided by /BIN
       programs (LS.EXE, CD.EXE, ...) which shadow these via
       DispatcherTryDiskCommand, so no kernel built-in is needed. */
    if (strcmp(Command->Command, "isolist") == 0 || strcmp(Command->Command, "lsiso") == 0) {
        CmdIsoList(); return;
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
    if (strcmp(Command->Command, "ncc") == 0) {
        CmdNcc(Command); return;
    }
    if (strcmp(Command->Command, "cat") == 0) {
        if (Command->ArgCount != 1U) { TerminalPrintLine32("Usage: cat <file>", 0x00FF0000); return; }
        CmdCat(Command->Args[0]); return;
    }
    if (strcmp(Command->Command, "n64a") == 0) {
        CmdN64a(Command); return;
    }
    if (strcmp(Command->Command, "nld") == 0) {
        CmdNld(Command); return;
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
    /* clear is provided by /BIN/CLEAR.EXE */
    if (strcmp(Command->Command, "panic") == 0){
        PanicKernel("Called Panic API For testing", (uintptr_t)PanicKernel);
        return;
    }
    if (strcmp(Command->Command, "DevKit") == 0 || strcmp(Command->Command, "devkit") == 0){
        CmdDevKit();
        return;
    }
    if (strcmp(Command->Command, "reboot") == 0){
        CmdRbt();
        return;
    }
    if (strcmp(Command->Command, "shutdown") == 0){
        CmdShdn();
        return;
    }
    /* where is provided by /BIN/WHERE.EXE */

    // --- Low-Level Memory & CPU Commands ---
    if (strcmp(Command->Command, "peek") == 0) { CmdPeek(Command); return; }
    if (strcmp(Command->Command, "poke") == 0) { CmdPoke(Command); return; }
    /* hexdump is provided by /BIN/HEXDUMP.EXE */
    if (strcmp(Command->Command, "inb") == 0) { CmdInB(Command); return; }
    if (strcmp(Command->Command, "outb") == 0) { CmdOutB(Command); return; }
    if (strcmp(Command->Command, "inw") == 0) { CmdInW(Command); return; }
    if (strcmp(Command->Command, "outw") == 0) { CmdOutW(Command); return; }
    if (strcmp(Command->Command, "ind") == 0) { CmdInD(Command); return; }
    if (strcmp(Command->Command, "outd") == 0) { CmdOutD(Command); return; }
    if (strcmp(Command->Command, "cpuid") == 0) { CmdCpuId(); return; }
    if (strcmp(Command->Command, "rdtsc") == 0) { CmdRdtsc(); return; }
    if (strcmp(Command->Command, "cr0") == 0) { CmdCR0(); return; }
    if (strcmp(Command->Command, "cr3") == 0) { CmdCR3(); return; }
    if (strcmp(Command->Command, "cr4") == 0) { CmdCR4(); return; }
    if (strcmp(Command->Command, "msr") == 0) { CmdMsr(Command); return; }
    if (strcmp(Command->Command, "diskread") == 0) { CmdDiskRead(Command); return; }

    // --- High-Level System & Utility Commands ---
    /* neofetch/sysinfo provided by /BIN/NEOFETCH.EXE */
    if (strcmp(Command->Command, "matrix") == 0) { CmdMatrix(); return; }
    if (strcmp(Command->Command, "calc") == 0) { CmdCalc(Command); return; }
    if (strcmp(Command->Command, "tree") == 0) { CmdTree(); return; }
    if (strcmp(Command->Command, "uptime") == 0) { CmdUptime(); return; }
    if (strcmp(Command->Command, "theme") == 0) { CmdTheme(Command); return; }
    if (strcmp(Command->Command, "isofind") == 0) {
        if (Command->ArgCount != 1U) { TerminalPrintLine32("Usage: isofind <name>", 0x00FF0000); return; }
        CmdIsoFind(Command->Args[0]); return;
    }
    if (strcmp(Command->Command, "isocat") == 0) {
        if (Command->ArgCount != 1U) { TerminalPrintLine32("Usage: isocat <path>", 0x00FF0000); return; }
        CmdIsoCat(Command->Args[0]); return;
    }
    if (strcmp(Command->Command, "isocopy") == 0) {
        if (Command->ArgCount != 2U) { TerminalPrintLine32("Usage: isocopy <isopath> <diskpath>", 0x00FF0000); return; }
        CmdIsoCopy(Command->Args[0], Command->Args[1]); return;
    }
    if (strcmp(Command->Command, "tt") == 0){
        CmdTickTestPIT();
        return;
    }
    if (strcmp(Command->Command, "sbep") == 0){
        if (Command->ArgCount != 2){
            TerminalPrintLine32("Invalid Args", PromColor);
            return;
        }
        uint64_t Frequ;
        uint64_t Last;
        ShellArgToUInt64(Command->Args[0], &Frequ);
        ShellArgToUInt64(Command->Args[1], &Last);
        CmdBep(Frequ, Last);
        return;
    }
    if (strcmp(Command->Command, "ebepe") == 0){
        if (Command->ArgCount != 0){
            TerminalPrintLine32("No Args Needed", PromColor);
            return;
        }
        CmdSBepEmrg();
        return;
    }
    if (strcmp(Command->Command, "smbep") == 0){
        if (Command->ArgCount != 12){
            TerminalPrintLine32("Invalid Args", PromColor);
            return;
        }
        uint64_t Frequ1, Frequ2, Frequ3, Frequ4, Frequ5, Frequ6;
        uint64_t Last1, Last2, Last3, Last4, Last5, Last6;
        ShellArgToUInt64(Command->Args[0], &Frequ1);
        ShellArgToUInt64(Command->Args[1], &Last1);
        ShellArgToUInt64(Command->Args[2], &Frequ2);
        ShellArgToUInt64(Command->Args[3], &Last2);
        ShellArgToUInt64(Command->Args[4], &Frequ3);
        ShellArgToUInt64(Command->Args[5], &Last3);
        ShellArgToUInt64(Command->Args[6], &Frequ4);
        ShellArgToUInt64(Command->Args[7], &Last4);
        ShellArgToUInt64(Command->Args[8], &Frequ5);
        ShellArgToUInt64(Command->Args[9], &Last5);
        ShellArgToUInt64(Command->Args[10], &Frequ6);
        ShellArgToUInt64(Command->Args[11], &Last6);
        CmdNSong(Frequ1, Last1, Frequ2, Last2, Frequ3, Last3, Frequ4, Last4, Frequ5, Last5, Frequ6, Last6);
        return;
    }

    TerminalWrite32("Kernel/shell/bin/cmd.c: ", PromColor);
    TerminalWrite32("Error: ", 0xFFFF0000);
    TerminalWrite32("Command Not found or Corrupted API: ", PromColor);
    TerminalPrintLine32(Command->Command, PromColor);
}