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
#include "../../arch/x86_64/power/pwr.h"
#include "fs/ISO9660/iso9660.h"
#include "drivers/storage/ide/ide.h"
#include "drivers/video_framebuffer/framebuffer.h"
#include "arch/x86_64/cpu/io.h"
#include "mm/heap/heap.h"
#include "kernel/panic/panic.h"
#include "../Kernel/drivers/timer/PIT.h"
#include "../../lib/sound/sndbeep.h"

static bool ParseNumber(const char *str, uint64_t *val)
{
    if (str == NULL || val == NULL || str[0] == '\0') return false;
    *val = 0;

    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        str += 2;
        if (str[0] == '\0') return false;
        while (*str)
        {
            char c = *str;
            uint64_t digit = 0;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else return false;
            *val = (*val << 4) | digit;
            str++;
        }
        return true;
    }

    while (*str)
    {
        char c = *str;
        if (c >= '0' && c <= '9')
        {
            *val = (*val * 10) + (c - '0');
        }
        else return false;
        str++;
    }
    return true;
}

void CmdHelp(void)
{

    TerminalPrintLine32("SYSTEM & DIAGNOSTICS", 0x00FFD700);
    TerminalPrintLine32("  help                            Show this epic command reference", PromColor);
    TerminalPrintLine32("  about                           Show OS version & architecture", PromColor);
    TerminalPrintLine32("  neofetch / sysinfo              Display full system spec logo banner", PromColor);
    TerminalPrintLine32("  clear                           Clear terminal screen", PromColor);
    TerminalPrintLine32("  devkit                          Show developer diagnostics", PromColor);
    TerminalPrintLine32("  suf <command>                   Trace source implementation of ANY command", PromColor);
    TerminalPrintLine32("  uptime                          Display CPU cycles and uptime", PromColor);
    TerminalPrintLine32("  matrix                          Run Matrix digital rain animation", PromColor);
    TerminalPrintLine32("  theme <name>                    Set color theme (cyberpunk/matrix/hacker/gold)", PromColor);
    TerminalPrintLine32("  reboot                          Reboot the system", PromColor);
    TerminalPrintLine32("  shutdown                        Shutdown the system (QEMU ACPI)", PromColor);

    TerminalPrintLine32("LOW-LEVEL MEMORY & CPU", 0x00FF00FF);
    TerminalPrintLine32("  peek <addr>                     Read 64-bit value at memory address", PromColor);
    TerminalPrintLine32("  poke <addr> <val>               Write 64-bit value to memory address", PromColor);
    TerminalPrintLine32("  hexdump <addr> [bytes]          Format memory hex dump with ASCII view", PromColor);
    TerminalPrintLine32("  inb / outb <port> [val]         Read / write 8-bit I/O port", PromColor);
    TerminalPrintLine32("  inw / outw <port> [val]         Read / write 16-bit I/O port", PromColor);
    TerminalPrintLine32("  ind / outd <port> [val]         Read / write 32-bit I/O port", PromColor);
    TerminalPrintLine32("  cpuid                           Query x86_64 CPU Vendor & Feature Flags", PromColor);
    TerminalPrintLine32("  rdtsc                           Read 64-bit CPU Time Stamp Counter", PromColor);
    TerminalPrintLine32("  cr0 / cr3 / cr4                 Read CPU Control Registers", PromColor);
    TerminalPrintLine32("  msr <msr_hex>                   Read 64-bit Model-Specific Register", PromColor);
    TerminalPrintLine32("  diskread <lba>                  Read & dump raw IDE/ATAPI sector", PromColor);
    TerminalPrintLine32("  sbep <Frequency> <Millisecond>  Let's beep together !", PromColor);
    TerminalPrintLine32("  ebepe                           Force Stop the beep sound", PromColor);
    TerminalPrintLine32("  nsong <F1> <L1> <F2> <L2> <F3> <L3> <F4> <L4> <F5> <L5> <F6> <L6>", PromColor);
    TerminalPrintLine32("                                  Play a 6-tone song with frequency & duration", PromColor);

    TerminalPrintLine32("FILESYSTEM & UTILITIES", 0x0057DB92);
    TerminalPrintLine32("  ls                              List VFS files", PromColor);
    TerminalPrintLine32("  touch <file>                    Create empty VFS file", PromColor);
    TerminalPrintLine32("  write <file> <text>             Write text to VFS file", PromColor);
    TerminalPrintLine32("  cat <file>                      Display VFS file content", PromColor);
    TerminalPrintLine32("  rm <file>                       Remove VFS file", PromColor);
    TerminalPrintLine32("  edit <file>                     Open NullEdit text editor", PromColor);
    TerminalPrintLine32("  tree                            Display tree structure of VFS & ISO", PromColor);
    TerminalPrintLine32("  calc <n1> <op> <n2>             Evaluate arithmetic & bitwise math", PromColor);

    TerminalPrintLine32("ISO9660 FILESYSTEM", 0x0000FFFF);
    TerminalPrintLine32("  isolist                         List files on ISO9660 media", PromColor);
    TerminalPrintLine32("  isofind <name>                  Search ISO9660 filesystem for file", PromColor);
    TerminalPrintLine32("  isocat <path>                   Read and print file directly from ISO", PromColor);
    TerminalPrintLine32("  isocopy <isopath> <vfspath>     Copy file from ISO9660 into VFS", PromColor);

    TerminalPrintLine32("REGISTRY API", 0x009AA8BD);
    TerminalPrintLine32("  where <API>                     Print kernel function memory address", PromColor);
    TerminalPrintLine32("  regout / regfill                Legacy text and color registry output", PromColor);
    TerminalPrintLine32("-------------------------------------------------", 0x00314A70);
}

void CmdDevKit(void){
    TerminalPrintLine32("  panic                        For Panic The kernel", PromColor);
    TerminalPrintLine32("  tt                           Timer Test", PromColor);
    TerminalPrintLine32("  suf <command>                Show command source dependencies", PromColor);
}

void CmdMsgFromNull(void){
    TerminalPrintLine32("Welcome to NullOS 0.5!", 0x00FF00FF);
    TerminalPrintLine32("NullOS created by Null, All rights reserved", 0x00FF00FF);
    TerminalPrintLine32("Created with hard work, hope, and countless sleepless nights!", 0x00FF00FF);
}

void CmdAbout(void){
    TerminalWrite32("Version: ", 0xFFFFFFFF);
    TerminalPrintLine32("NullOS 0.5", 0x00FF00FF);
    TerminalWrite32("Kernel: ", 0xFFFFFFFF);
    TerminalPrintLine32("Null Kernel 0.5.0", 0x00FF00FF);
    TerminalWrite32("Architecture: ", 0xFFFFFFFF);
    TerminalPrintLine32("x86_64 Long Mode", 0x00FF00FF);
    TerminalWrite32("Bootloader: ", 0xFFFFFFFF);
    TerminalPrintLine32("Limine BIOS/UEFI", 0x00FF00FF);
}

void CmdFill(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount != 2)
    {
        TerminalPrintLine32("Usage: regfill [FG] [BG]", 0xFFFFFFFF);
        return;
    }

    uint32_t ColorFG = 0xFFFFFFFF;
    uint32_t ColorBG = 0x00000000;

    if (!StringHexToUInt32(Command->Args[0], &ColorFG))
    {
        TerminalWrite32("Invalid Foreground Color: ", 0x00FF0000);
        TerminalPrintLine32("Required ARGB-Hex Color", PromColor);
        return;
    }

    if (!StringHexToUInt32(Command->Args[1], &ColorBG))
    {
        TerminalPrintLine32("Invalid Background Color: ", 0x00FF0000);
        TerminalPrintLine32("Required ARGB-Hex Color", PromColor);
        return;
    }

    TerminalClear32(ColorBG);
    PromColor = ColorFG;
    BackSpaceColor = ColorBG;
}

void CmdNewLine(void){
}

void CmdPurge(void){
    TerminalClear32(BackSpaceColor);
}

void CmdEcho(const PARSED_COMMAND *command)
{
    if (command->ArgCount < 1)
    {
        TerminalNewLine32(0);
        return;
    }

    for (uint32_t i = 0; i < command->ArgCount; i++)
    {
        TerminalWrite32(command->Args[i], PromColor);
        if (i + 1 < command->ArgCount)
            TerminalWrite32(" ", PromColor);
    }
    TerminalNewLine32(0);
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
        for (uint32_t Index = 0; Part[Index] != '\0' && Position < sizeof(Text) - 1U; Index++)
            Text[Position++] = Part[Index];
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
    if (CommandName == NULL || CommandName[0] == '\0')
    {
        TerminalPrintLine32("Usage: suf <command>", 0x00FF5C77);
        return;
    }

    TerminalPrintLine32("NULLOS DYNAMIC SOURCE CATALOG (SUF)", 0x005AA9FF);
    TerminalPrintLine32("--------------------------------------------------", 0x00314A70);
    TerminalWrite32("COMMAND: ", 0x0057DB92);
    TerminalPrintLine32(CommandName, 0x00E8F0FF);

    CmdSufLine("1. Input Event Interrupt   -> drivers/input/keyboard/keyboard.c");
    CmdSufLine("2. Command Lexer / Parser  -> shell/parser.c");
    CmdSufLine("3. Command Dispatcher      -> shell/dispatcher.c");
    CmdSufLine("4. Command Implementation  -> shell/commands/cmd.c");
    CmdSufLine("5. Core Subsystem Path:");

    if (strcmp(CommandName, "peek") == 0 || strcmp(CommandName, "poke") == 0 || strcmp(CommandName, "hexdump") == 0)
    {
        CmdSufLine("   shell/commands/cmd.c -> Volatile 64-bit MMIO pointer dereference & raw memory access");
    }
    else if (strcmp(CommandName, "inb") == 0 || strcmp(CommandName, "outb") == 0 || strcmp(CommandName, "inw") == 0 ||
             strcmp(CommandName, "outw") == 0 || strcmp(CommandName, "ind") == 0 || strcmp(CommandName, "outd") == 0)
    {
        CmdSufLine("   arch/x86_64/cpu/io.h & io.c -> Inline x86_64 Port I/O Assembly (inb/outb/inw/outw/ind/outd)");
    }
    else if (strcmp(CommandName, "cpuid") == 0 || strcmp(CommandName, "rdtsc") == 0 || strcmp(CommandName, "cr0") == 0 ||
             strcmp(CommandName, "cr3") == 0 || strcmp(CommandName, "cr4") == 0 || strcmp(CommandName, "msr") == 0)
    {
        CmdSufLine("   arch/x86_64 instructions -> cpuid, rdtsc, mov crX, rdmsr inline asm");
    }
    else if (strcmp(CommandName, "diskread") == 0)
    {
        CmdSufLine("   drivers/storage/ide/ide.c -> IDEReadSector() (ATA PIO / ATAPI SCSI READ 10)");
    }
    else if (strcmp(CommandName, "ls") == 0 || strcmp(CommandName, "touch") == 0 || strcmp(CommandName, "cat") == 0 ||
             strcmp(CommandName, "write") == 0 || strcmp(CommandName, "rm") == 0 || strcmp(CommandName, "edit") == 0)
    {
        CmdSufLine("   fs/vfs/vfs.c & fs/ramfs/ramfs.c -> Virtual Filesystem Engine");
    }
    else if (strcmp(CommandName, "isolist") == 0 || strcmp(CommandName, "lsiso") == 0 || strcmp(CommandName, "isofind") == 0 || strcmp(CommandName, "isocat") == 0)
    {
        CmdSufLine("   fs/ISO9660/iso9660.c -> Primary Volume Descriptor & Directory Parser");
    }
    else if (strcmp(CommandName, "clear") == 0 || strcmp(CommandName, "matrix") == 0 || strcmp(CommandName, "theme") == 0 ||
             strcmp(CommandName, "neofetch") == 0 || strcmp(CommandName, "sysinfo") == 0)
    {
        CmdSufLine("   kernel/terminal/terminal.c & drivers/video_framebuffer/framebuffer.c -> Framebuffer Graphics Engine");
    }
    else if (strcmp(CommandName, "reboot") == 0 || strcmp(CommandName, "shutdown") == 0)
    {
        CmdSufLine("   arch/x86_64/power/pwr.c & pwr.h -> System Power State & Hardware Reset");
    }
    else if (strcmp(CommandName, "uptime") == 0)
    {
        CmdSufLine("   arch/x86_64 instructions -> rdtsc CPU Time Stamp Counter & System Uptime");
    }
    else if (strcmp(CommandName, "calc") == 0)
    {
        CmdSufLine("   shell/commands/cmd.c -> CmdCalc() Arithmetic & bitwise math evaluator");
    }
    else if (strcmp(CommandName, "tree") == 0)
    {
        CmdSufLine("   fs/vfs/vfs.c & fs/ISO9660/iso9660.c -> Hierarchical tree directory walker");
    }
    else if (strcmp(CommandName, "panic") == 0)
    {
        CmdSufLine("   kernel/panic/panic.c -> PanicKernel() software kernel panic");
    }
    else if (strcmp(CommandName, "where") == 0)
    {
        CmdSufLine("   shell/commands/cmd.c -> CmdShowAddr() Dynamic symbol address tracer");
    }
    else if (strcmp(CommandName, "regout") == 0 || strcmp(CommandName, "RegOut") == 0 ||
             strcmp(CommandName, "regfill") == 0 || strcmp(CommandName, "RegFill") == 0 ||
             strcmp(CommandName, "regcreate") == 0 || strcmp(CommandName, "delreg") == 0 ||
             strcmp(CommandName, "fregout") == 0 || strcmp(CommandName, "regin") == 0)
    {
        CmdSufLine("   fs/ramfs/ramfs.c & shell/commands/cmd.c -> RamFS registry database & API");
    }
    else if (strcmp(CommandName, "help") == 0 || strcmp(CommandName, "about") == 0 ||
             strcmp(CommandName, "devkit") == 0 || strcmp(CommandName, "DevKit") == 0 ||
             strcmp(CommandName, "m5sfr0mnu11") == 0 || strcmp(CommandName, "suf") == 0)
    {
        CmdSufLine("   shell/commands/cmd.c -> System & diagnostic reference functions");
    }
    else
    {
        CmdSufLine("   shell/commands/cmd.c -> Generic system API dispatcher");
    }
}

void CmdEdit(const char *Path)
{
    if (!EditorOpen(Path))
    {
        TerminalPrintLine32("edit: cannot open file", 0x00FF5C77);
    }
}

void CmdRbt(void){
    Reboot();
}

void CmdShdn(void){
    Shutdown();
}

void CmdShowAddr(const char *API)
{
    if (strcmp(API, "KernelMainAPI") == 0)
    {
        extern void kernel_main(void);
        TerminalWrite32("kernel_main       -> ", PromColor);
        TerminalPrintHex64Compact((uintptr_t)kernel_main);
        TerminalNewLine32(BackSpaceColor);
        return;
    }
    if (strcmp(API, "KBDInit") == 0)
    {
        extern void KbdInitialize(void);
        TerminalWrite32("KbdInitialize     -> ", PromColor);
        TerminalPrintHex64Compact((uintptr_t)KbdInitialize);
        TerminalNewLine32(BackSpaceColor);
        return;
    }
    if (strcmp(API, "Inb") == 0)
    {
        TerminalWrite32("Inb               -> ", PromColor);
        TerminalPrintHex64Compact((uintptr_t)Inb);
        TerminalNewLine32(BackSpaceColor);
        return;
    }
    if (strcmp(API, "Outb") == 0)
    {
        TerminalWrite32("Outb              -> ", PromColor);
        TerminalPrintHex64Compact((uintptr_t)Outb);
        TerminalNewLine32(BackSpaceColor);
        return;
    }
    if (strcmp(API, "IDEReadSector") == 0)
    {
        TerminalWrite32("IDEReadSector     -> ", PromColor);
        TerminalPrintHex64Compact((uintptr_t)IDEReadSector);
        TerminalNewLine32(BackSpaceColor);
        return;
    }
    TerminalPrintLine32("Cannot Access the API, Might not found", PromColor);
}

void CmdIsoList(void)
{
    ISODirRecord Root;
    if (ISOGetRootDir(&Root))
    {
        ISOReadDirectory(&Root);
    }
    else
    {
        TerminalPrintLine32("ISO9660 filesystem not loaded", 0xFFFF0000);
    }
}

// Low-Level Memory & CPU Commands
void CmdPeek(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 1)
    {
        TerminalPrintLine32("Usage: peek <addr_hex>", 0xFFFF0000);
        return;
    }
    uint64_t addr = 0;
    if (!ParseNumber(Command->Args[0], &addr))
    {
        TerminalPrintLine32("peek: invalid memory address", 0xFFFF0000);
        return;
    }

    uint64_t val = *(volatile uint64_t *)addr;
    TerminalWrite32("[", PromColor);
    TerminalPrintHex64Compact(addr);
    TerminalWrite32("] = ", PromColor);
    TerminalPrintHex64Compact(val);
    TerminalNewLine32(0);
}

void CmdPoke(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 2)
    {
        TerminalPrintLine32("Usage: poke <addr_hex> <val_hex>", 0xFFFF0000);
        return;
    }
    uint64_t addr = 0, val = 0;
    if (!ParseNumber(Command->Args[0], &addr) || !ParseNumber(Command->Args[1], &val))
    {
        TerminalPrintLine32("poke: invalid arguments", 0xFFFF0000);
        return;
    }

    *(volatile uint64_t *)addr = val;
    TerminalWrite32("Poked [", PromColor);
    TerminalPrintHex64Compact(addr);
    TerminalWrite32("] <= ", PromColor);
    TerminalPrintHex64Compact(val);
    TerminalNewLine32(0);
}

void CmdHexDump(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 1)
    {
        TerminalPrintLine32("Usage: hexdump <addr_hex> [bytes]", 0xFFFF0000);
        return;
    }
    uint64_t addr = 0;
    uint64_t count = 64;
    if (!ParseNumber(Command->Args[0], &addr))
    {
        TerminalPrintLine32("hexdump: invalid address", 0xFFFF0000);
        return;
    }
    if (Command->ArgCount >= 2)
    {
        ParseNumber(Command->Args[1], &count);
    }
    if (count > 512) count = 512;

    const uint8_t *ptr = (const uint8_t *)addr;
    for (uint64_t i = 0; i < count; i += 16)
    {
        TerminalPrintHex64Compact(addr + i);
        TerminalWrite32(": ", 0x0057DB92);

        for (uint64_t j = 0; j < 16; j++)
        {
            if (i + j < count)
            {
                uint8_t byte = ptr[i + j];
                TerminalPrintHex64Compact(byte);
                TerminalWrite32(" ", PromColor);
            }
            else
            {
                TerminalWrite32("   ", PromColor);
            }
        }
        TerminalWrite32(" |", 0x00314A70);
        for (uint64_t j = 0; j < 16; j++)
        {
            if (i + j < count)
            {
                char c = ptr[i + j];
                if (c >= 32 && c <= 126)
                {
                    char s[2] = {c, '\0'};
                    TerminalWrite32(s, 0x0000FF00);
                }
                else
                {
                    TerminalWrite32(".", 0x00555555);
                }
            }
        }
        TerminalPrintLine32("|", 0x00314A70);
    }
}

void CmdInB(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 1) { TerminalPrintLine32("Usage: inb <port_hex>", 0xFFFF0000); return; }
    uint64_t port = 0;
    if (!ParseNumber(Command->Args[0], &port)) { TerminalPrintLine32("inb: invalid port", 0xFFFF0000); return; }
    uint8_t val = Inb((uint16_t)port);
    TerminalWrite32("inb(", PromColor); TerminalPrintHex64Compact(port); TerminalWrite32(") = ", PromColor);
    TerminalPrintHex64Compact(val); TerminalNewLine32(0);
}

void CmdOutB(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 2) { TerminalPrintLine32("Usage: outb <port_hex> <val_hex>", 0xFFFF0000); return; }
    uint64_t port = 0, val = 0;
    if (!ParseNumber(Command->Args[0], &port) || !ParseNumber(Command->Args[1], &val)) { TerminalPrintLine32("outb: invalid args", 0xFFFF0000); return; }
    Outb((uint16_t)port, (uint8_t)val);
    TerminalPrintLine32("outb OK", 0x0057DB92);
}

void CmdInW(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 1) { TerminalPrintLine32("Usage: inw <port_hex>", 0xFFFF0000); return; }
    uint64_t port = 0;
    if (!ParseNumber(Command->Args[0], &port)) { TerminalPrintLine32("inw: invalid port", 0xFFFF0000); return; }
    uint16_t val = Inw((uint16_t)port);
    TerminalWrite32("inw(", PromColor); TerminalPrintHex64Compact(port); TerminalWrite32(") = ", PromColor);
    TerminalPrintHex64Compact(val); TerminalNewLine32(0);
}

void CmdOutW(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 2) { TerminalPrintLine32("Usage: outw <port_hex> <val_hex>", 0xFFFF0000); return; }
    uint64_t port = 0, val = 0;
    if (!ParseNumber(Command->Args[0], &port) || !ParseNumber(Command->Args[1], &val)) { TerminalPrintLine32("outw: invalid args", 0xFFFF0000); return; }
    Outw((uint16_t)port, (uint16_t)val);
    TerminalPrintLine32("outw OK", 0x0057DB92);
}

void CmdInD(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 1) { TerminalPrintLine32("Usage: ind <port_hex>", 0xFFFF0000); return; }
    uint64_t port = 0;
    if (!ParseNumber(Command->Args[0], &port)) { TerminalPrintLine32("ind: invalid port", 0xFFFF0000); return; }
    uint32_t val = Ind((uint16_t)port);
    TerminalWrite32("ind(", PromColor); TerminalPrintHex64Compact(port); TerminalWrite32(") = ", PromColor);
    TerminalPrintHex64Compact(val); TerminalNewLine32(0);
}

void CmdOutD(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 2) { TerminalPrintLine32("Usage: outd <port_hex> <val_hex>", 0xFFFF0000); return; }
    uint64_t port = 0, val = 0;
    if (!ParseNumber(Command->Args[0], &port) || !ParseNumber(Command->Args[1], &val)) { TerminalPrintLine32("outd: invalid args", 0xFFFF0000); return; }
    Outd((uint16_t)port, (uint32_t)val);
    TerminalPrintLine32("outd OK", 0x0057DB92);
}

void CmdCpuId(void)
{
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));

    char vendor[13];
    *(uint32_t *)(vendor) = ebx;
    *(uint32_t *)(vendor + 4) = edx;
    *(uint32_t *)(vendor + 8) = ecx;
    vendor[12] = '\0';

    TerminalWrite32("CPU Vendor  : ", 0x0057DB92);
    TerminalPrintLine32(vendor, 0x0000FF00);

    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    TerminalWrite32("Features EDX: ", 0x0057DB92); TerminalPrintHex64Compact(edx); TerminalNewLine32(0);
    TerminalWrite32("Features ECX: ", 0x0057DB92); TerminalPrintHex64Compact(ecx); TerminalNewLine32(0);
}

void CmdRdtsc(void)
{
    uint32_t lo = 0, hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t tsc = ((uint64_t)hi << 32) | lo;

    TerminalWrite32("TSC Cycles : ", 0x0057DB92);
    TerminalPrintHex64Compact(tsc);
    TerminalNewLine32(0);
}

void CmdCR0(void)
{
    uint64_t cr0 = 0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    TerminalWrite32("CR0 Register : ", 0x0057DB92);
    TerminalPrintHex64Compact(cr0);
    TerminalNewLine32(0);
}

void CmdCR3(void)
{
    uint64_t cr3 = 0;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    TerminalWrite32("CR3 (Page Table Base) : ", 0x0057DB92);
    TerminalPrintHex64Compact(cr3);
    TerminalNewLine32(0);
}

void CmdCR4(void)
{
    uint64_t cr4 = 0;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    TerminalWrite32("CR4 Register : ", 0x0057DB92);
    TerminalPrintHex64Compact(cr4);
    TerminalNewLine32(0);
}

void CmdMsr(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 1)
    {
        TerminalPrintLine32("Usage: msr <msr_hex>", 0xFFFF0000);
        return;
    }
    uint64_t msr = 0;
    if (!ParseNumber(Command->Args[0], &msr))
    {
        TerminalPrintLine32("msr: invalid msr index", 0xFFFF0000);
        return;
    }
    uint32_t lo = 0, hi = 0;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"((uint32_t)msr));
    uint64_t val = ((uint64_t)hi << 32) | lo;
    TerminalWrite32("MSR[", PromColor);
    TerminalPrintHex64Compact(msr);
    TerminalWrite32("] = ", PromColor);
    TerminalPrintHex64Compact(val);
    TerminalNewLine32(0);
}

void CmdDiskRead(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 1)
    {
        TerminalPrintLine32("Usage: diskread <lba_hex>", 0xFFFF0000);
        return;
    }
    uint64_t lba = 0;
    if (!ParseNumber(Command->Args[0], &lba))
    {
        TerminalPrintLine32("diskread: invalid LBA", 0xFFFF0000);
        return;
    }

    uint8_t buf[2048];
    memset(buf, 0, sizeof(buf));
    if (!IDEReadSector((uint32_t)lba, buf))
    {
        TerminalPrintLine32("diskread: sector read failed", 0xFFFF0000);
        return;
    }

    TerminalWrite32("Sector LBA ", 0x0057DB92);
    TerminalPrintHex64Compact(lba);
    TerminalPrintLine32(" Raw Header:", 0x0057DB92);
    for (int i = 0; i < 64; i += 16)
    {
        TerminalPrintHex64Compact((uint64_t)i);
        TerminalWrite32(": ", 0x00314A70);
        for (int j = 0; j < 16; j++)
        {
            TerminalPrintHex64Compact(buf[i + j]);
            TerminalWrite32(" ", PromColor);
        }
        TerminalNewLine32(0);
    }
}

void CmdNeofetch(void)
{
    TerminalPrintLine32("       _   _       _ _  ____   _____ ", 0x0000FFFF);
    TerminalPrintLine32("      | \\ | |     | | |/ __ \\ / ____|", 0x005AA9FF);
    TerminalPrintLine32("      |  \\| |_   _| | | |  | | (___  ", 0x0057DB92);
    TerminalPrintLine32("      | . ` | | | | | | |  | |\\___ \\ ", 0x00FFD700);
    TerminalPrintLine32("      | |\\  | |_| | | | |__| |____) |", 0x00FF00FF);
    TerminalPrintLine32("      |_| \\_|\\__,_|_|_|\\____/|_____/ ", 0x00FF0000);
    TerminalPrintLine32("-------------------------------------------------", 0x00314A70);
    TerminalWrite32("  OS           : ", 0x0057DB92); TerminalPrintLine32("NullOS 0.5 (x86_64 Edition)", PromColor);
    TerminalWrite32("  Kernel       : ", 0x0057DB92); TerminalPrintLine32("NullKernel 0.5.0", PromColor);
    TerminalWrite32("  Architecture : ", 0x0057DB92); TerminalPrintLine32("x86_64 Long Mode", PromColor);
    TerminalWrite32("  Bootloader   : ", 0x0057DB92); TerminalPrintLine32("Limine BIOS/UEFI", PromColor);
    TerminalWrite32("  Display      : ", 0x0057DB92);
    TerminalPrintHex64Compact(FbGetWidth32());
    TerminalWrite32("x", PromColor);
    TerminalPrintHex64Compact(FbGetHeight32());
    TerminalPrintLine32(" 32-bpp Framebuffer", PromColor);
    TerminalWrite32("  Storage      : ", 0x0057DB92);
    if (IDEIsATAPI()) TerminalPrintLine32("IDE ATAPI CD-ROM (Active)", 0x0000FFFF);
    else TerminalPrintLine32("IDE ATA Hard Disk (Active)", 0x0057DB92);
    TerminalWrite32("  Filesystem   : ", 0x0057DB92); TerminalPrintLine32("ISO9660 + RamFS VFS", PromColor);
    TerminalWrite32("  Shell        : ", 0x0057DB92); TerminalPrintLine32("Null Shell", 0x00FF00FF);
    TerminalPrintLine32("-------------------------------------------------", 0x00314A70);
}

void CmdMatrix(void)
{
    TerminalPrintLine32("Entering Matrix Digital Rain... Press any key to return.", 0x0000FF00);
    uint32_t width = FbGetWidth32();
    uint32_t height = FbGetHeight32();

    FbCleanScreen32(0x00000000);

    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ@#$%&*";
    uint32_t charcount = sizeof(charset) - 1;

    for (uint32_t step = 0; step < 120; step++)
    {
        for (uint32_t col = 0; col < width; col += 16)
        {
            uint32_t row = ((step * 13 + col * 7) % (height / 16)) * 16;
            char c = charset[(step + col) % charcount];
            uint32_t color = ((step % 2) == 0) ? 0x0000FF00 : 0x00008800;
            FbDrawChar32(c, col, row, color);
        }
        for (volatile uint32_t d = 0; d < 2000000; d++);
    }

    TerminalClear32(BackSpaceColor);
    TerminalPrintLine32("Returned from Matrix.", PromColor);
}

void CmdCalc(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 3)
    {
        TerminalPrintLine32("Usage: calc <num1> <op> <num2>", 0xFFFF0000);
        TerminalPrintLine32("Operators: + - * / % & | ^ << >>", PromColor);
        return;
    }
    uint64_t n1 = 0, n2 = 0;
    if (!ParseNumber(Command->Args[0], &n1) || !ParseNumber(Command->Args[2], &n2))
    {
        TerminalPrintLine32("calc: invalid numbers", 0xFFFF0000);
        return;
    }
    const char *op = Command->Args[1];
    uint64_t res = 0;

    if (strcmp(op, "+") == 0) res = n1 + n2;
    else if (strcmp(op, "-") == 0) res = n1 - n2;
    else if (strcmp(op, "*") == 0) res = n1 * n2;
    else if (strcmp(op, "/") == 0) { if (n2 == 0) { TerminalPrintLine32("calc: div by zero", 0xFFFF0000); return; } res = n1 / n2; }
    else if (strcmp(op, "%") == 0) { if (n2 == 0) { TerminalPrintLine32("calc: mod by zero", 0xFFFF0000); return; } res = n1 % n2; }
    else if (strcmp(op, "&") == 0) res = n1 & n2;
    else if (strcmp(op, "|") == 0) res = n1 | n2;
    else if (strcmp(op, "^") == 0) res = n1 ^ n2;
    else if (strcmp(op, "<<") == 0) res = n1 << n2;
    else if (strcmp(op, ">>") == 0) res = n1 >> n2;
    else { TerminalPrintLine32("calc: unknown operator", 0xFFFF0000); return; }

    TerminalWrite32("Result: ", 0x0057DB92);
    TerminalPrintHex64Compact(res);
    TerminalNewLine32(0);
}

void CmdTree(void)
{
    TerminalPrintLine32(".", 0x0057DB92);
    TerminalPrintLine32("├── [VFS Filesystem]", 0x005AA9FF);
    CmdLs();
    TerminalPrintLine32("└── [ISO9660 Media]", 0x0000FFFF);
    ISODirRecord Root;
    if (ISOGetRootDir(&Root))
    {
        ISOReadDirectory(&Root);
    }
    else
    {
        TerminalPrintLine32("    (ISO Media Not Loaded)", 0x00555555);
    }
}

void CmdUptime(void)
{
    uint32_t lo = 0, hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t tsc = ((uint64_t)hi << 32) | lo;

    TerminalWrite32("System Uptime Clock Cycles: ", 0x0057DB92);
    TerminalPrintHex64Compact(tsc);
    TerminalNewLine32(0);
}

void CmdTheme(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 1)
    {
        TerminalPrintLine32("Usage: theme <matrix|cyberpunk|hacker|gold|classic|dark>", 0xFFFF0000);
        return;
    }
    const char *name = Command->Args[0];
    if (strcasecmp(name, "matrix") == 0) { PromColor = 0x0000FF00; BackSpaceColor = 0x00000000; }
    else if (strcasecmp(name, "cyberpunk") == 0) { PromColor = 0x00FF00FF; BackSpaceColor = 0x00000033; }
    else if (strcasecmp(name, "hacker") == 0) { PromColor = 0x0033FF33; BackSpaceColor = 0x00050505; }
    else if (strcasecmp(name, "gold") == 0) { PromColor = 0x00FFD700; BackSpaceColor = 0x00100010; }
    else if (strcasecmp(name, "classic") == 0) { PromColor = 0x00FFFFFF; BackSpaceColor = 0x00000000; }
    else if (strcasecmp(name, "dark") == 0) { PromColor = 0x00CCCCCC; BackSpaceColor = 0x001A1A1A; }
    else { TerminalPrintLine32("theme: unknown theme name", 0xFFFF0000); return; }

    TerminalClear32(BackSpaceColor);
    TerminalPrintLine32("Theme updated successfully!", PromColor);
}

void CmdIsoFind(const char *Name)
{
    ISODirRecord Root, FileRec;
    if (!ISOGetRootDir(&Root)) { TerminalPrintLine32("isofind: ISO9660 not loaded", 0xFFFF0000); return; }
    if (ISOFindFile(&Root, Name, &FileRec) || ISOFindPath(Name, &FileRec))
    {
        TerminalPrintLine32("ISO File Found!", 0x0057DB92);
        TerminalWrite32("Name: ", PromColor); TerminalPrintLine32(FileRec.FileIdentifier, PromColor);
        TerminalWrite32("LBA : ", PromColor); TerminalPrintHex64Compact(FileRec.ExtentLBA); TerminalNewLine32(0);
        TerminalWrite32("Size: ", PromColor); TerminalPrintHex64Compact(FileRec.DataLength); TerminalNewLine32(0);
    }
    else
    {
        TerminalPrintLine32("isofind: File not found on ISO", 0xFFFF0000);
    }
}

void CmdIsoCat(const char *Path)
{
    ISODirRecord FileRec;
    if (!ISOFindPath(Path, &FileRec)) { TerminalPrintLine32("isocat: File not found on ISO", 0xFFFF0000); return; }

    if (FileRec.DataLength == 0) { TerminalPrintLine32("(Empty ISO File)", PromColor); return; }

    uint8_t *buf = (uint8_t *)kmalloc(FileRec.DataLength + 1);
    if (buf == NULL) { TerminalPrintLine32("isocat: Heap allocation failed", 0xFFFF0000); return; }

    uint32_t bytesRead = 0;
    if (!ISOReadFile(&FileRec, buf, FileRec.DataLength, &bytesRead))
    {
        TerminalPrintLine32("isocat: ISO read error", 0xFFFF0000);
        return;
    }
    buf[bytesRead] = '\0';
    TerminalPrintLine32((const char *)buf, PromColor);
}

void CmdIsoCopy(const char *IsoPath, const char *VfsPath)
{
    ISODirRecord FileRec;
    if (!ISOFindPath(IsoPath, &FileRec))
    {
        TerminalPrintLine32("isocopy: Source file not found on ISO", 0xFFFF0000);
        return;
    }

    if (FileRec.DataLength == 0)
    {
        if (VfsWrite(VfsPath, "", 0))
            TerminalPrintLine32("isocopy: Successfully copied to VFS!", 0x0057DB92);
        return;
    }

    uint8_t *buf = (uint8_t *)kmalloc(FileRec.DataLength);
    if (buf == NULL)
    {
        TerminalPrintLine32("isocopy: Memory allocation failed", 0xFFFF0000);
        return;
    }

    uint32_t bytesRead = 0;
    if (!ISOReadFile(&FileRec, buf, FileRec.DataLength, &bytesRead))
    {
        TerminalPrintLine32("isocopy: Failed to read file from ISO", 0xFFFF0000);
        return;
    }

    if (VfsWrite(VfsPath, buf, bytesRead))
    {
        TerminalPrintLine32("isocopy: Successfully copied to VFS!", 0x0057DB92);
    }
}

void CmdTickTestPIT(void){
    PITTest();
}

void CmdBep(uint64_t Frequency, uint32_t Last){
    StartBeep(Frequency, Last);
}
void CmdSBepEmrg(void){
    EndBeepEmrg();
}

void CmdNSong(uint64_t Freq1, uint32_t Last1, uint64_t Freq2, uint32_t Last2, uint64_t Freq3, uint32_t Last3, uint64_t Freq4, uint32_t Last4, uint64_t Freq5, uint32_t Last5, uint64_t Freq6, uint32_t Last6){
    StartBeep(Freq1, Last1);
    StartBeep(Freq2, Last2);
    StartBeep(Freq3, Last3);
    StartBeep(Freq4, Last4);
    StartBeep(Freq5, Last5);
    StartBeep(Freq6, Last6);
}