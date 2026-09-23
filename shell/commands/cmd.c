#include "cmd.h"
#include "kernel/terminal/terminal.h"
#include "../parser.h"
#include <stdint.h>
#include <stddef.h>
#include "lib/string/string.h"
#include "mm/heap/heap.h"
#include "../shell.h"
#include "fs/ramfs/ramfs.h"
#include "include/diskfs.h"
#include "shell/editor/editor.h"
#include "libc/stdlib.h"
#include "libc/string.h"
#include "ncc/ncc.h"
#include "n64a/n64a.h"
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

void CmdHelp(const char *Category)
{
    if (Category == NULL || Category[0] == '\0' ||
        strcasecmp(Category, "all") == 0)
    {
        TerminalPrintLine32("NULLOS HELP  -  type 'help <category>' for details", 0x00FFD700);
        TerminalPrintLine32("-------------------------------------------------", 0x00314A70);
        TerminalPrintLine32("  help system                    System & diagnostics commands", PromColor);
        TerminalPrintLine32("  help filesystem                Disk file & directory tools", PromColor);
        TerminalPrintLine32("  help memory                    Low-level memory & CPU commands", PromColor);
        TerminalPrintLine32("  help iso                       ISO9660 media commands", PromColor);
        TerminalPrintLine32("  help registry                  RAM registry database commands", PromColor);
        TerminalPrintLine32("  help sound                     Sound & beep commands", PromColor);
        TerminalPrintLine32("  help developer                 Developer diagnostics commands", PromColor);
        TerminalPrintLine32("  help lowlevel                  x86_64 CPU / bus / port / memory lab", PromColor);
        TerminalPrintLine32("-------------------------------------------------", 0x00314A70);
        return;
    }

    if (strcasecmp(Category, "system") == 0)
    {
        TerminalPrintLine32("SYSTEM & DIAGNOSTICS", 0x00FFD700);
        TerminalPrintLine32("  help                            Show this command reference", PromColor);
        TerminalPrintLine32("  about                           Show OS version & architecture", PromColor);
        TerminalPrintLine32("  neofetch / sysinfo              Display full system spec logo banner", PromColor);
        TerminalPrintLine32("  clear                           Clear terminal screen", PromColor);
        TerminalPrintLine32("  uptime                          Display CPU cycles and uptime", PromColor);
        TerminalPrintLine32("  matrix                          Run Matrix digital rain animation", PromColor);
        TerminalPrintLine32("  theme <name>                    Set color theme (cyberpunk/matrix/hacker/gold)", PromColor);
        TerminalPrintLine32("  calc <n1> <op> <n2>             Evaluate arithmetic & bitwise math", PromColor);
        TerminalPrintLine32("  reboot                          Reboot the system", PromColor);
        TerminalPrintLine32("  shutdown                        Shutdown the system (QEMU ACPI)", PromColor);
        return;
    }

    if (strcasecmp(Category, "filesystem") == 0)
    {
        TerminalPrintLine32("FILESYSTEM (FAT32 disk tools)", 0x0057DB92);
        TerminalPrintLine32("  ls                              List files in current directory", PromColor);
        TerminalPrintLine32("  cd <dir>                        Change directory ('..' goes up)", PromColor);
        TerminalPrintLine32("  pwd                             Print current working directory", PromColor);
        TerminalPrintLine32("  mkdir <dir>                     Create a directory", PromColor);
        TerminalPrintLine32("  rmdir <dir>                     Remove a directory", PromColor);
        TerminalPrintLine32("  touch <file>                    Create empty file", PromColor);
        TerminalPrintLine32("  write <file> <text>             Write text to file", PromColor);
        TerminalPrintLine32("  cat <file>                      Display file content", PromColor);
        TerminalPrintLine32("  rm <file>                       Remove file", PromColor);
        TerminalPrintLine32("  edit <file>                     Open NullEdit text editor", PromColor);
        TerminalPrintLine32("  tree                            Display directory tree", PromColor);
        TerminalPrintLine32("  cp <src> <dst>                  Copy file or directory (recursive)", PromColor);
        return;
    }

    if (strcasecmp(Category, "memory") == 0)
    {
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
        return;
    }

    if (strcasecmp(Category, "iso") == 0)
    {
        TerminalPrintLine32("ISO9660 FILESYSTEM", 0x0000FFFF);
        TerminalPrintLine32("  isolist                         List files on ISO9660 media", PromColor);
        TerminalPrintLine32("  isofind <name>                  Search ISO9660 filesystem for file", PromColor);
        TerminalPrintLine32("  isocat <path>                   Read and print file directly from ISO", PromColor);
        TerminalPrintLine32("  isocopy <isopath> <diskpath>    Copy file from ISO9660 to disk", PromColor);
        return;
    }

    if (strcasecmp(Category, "registry") == 0)
    {
        TerminalPrintLine32("REGISTRY API (COLOR)", 0x009AA8BD);
        TerminalPrintLine32("  regfill <fg> <bg>               Set terminal FG/BG color (saves /REG/color)", PromColor);
        return;
    }

    if (strcasecmp(Category, "sound") == 0)
    {
        TerminalPrintLine32("SOUND & BEEP", 0x00FF8800);
        TerminalPrintLine32("  sbep <Freq> <Ms>                Let's beep together !", PromColor);
        TerminalPrintLine32("  ebepe                           Force Stop the beep sound", PromColor);
        TerminalPrintLine32("  smbep <6x freq+ms pairs>        Play a 6-tone song", PromColor);
        return;
    }

    if (strcasecmp(Category, "developer") == 0)
    {
        TerminalPrintLine32("DEVELOPER DIAGNOSTICS", 0x00FF5C77);
        TerminalPrintLine32("  devkit                          Show developer diagnostics", PromColor);
        TerminalPrintLine32("  suf <command>                   Trace source implementation of ANY command", PromColor);
        TerminalPrintLine32("  where <API>                     Print kernel function memory address", PromColor);
        TerminalPrintLine32("  panic                           Panic the kernel (testing)", PromColor);
        TerminalPrintLine32("  tt                              Timer test", PromColor);
        TerminalPrintLine32("  m5sfr0mnu11                     Hidden message", PromColor);
        return;
    }

    if (strcasecmp(Category, "lowlevel") == 0 || strcasecmp(Category, "low-level") == 0)
    {
        TerminalPrintLine32("LOW-LEVEL LAB / x86_64", 0x00FFD700);
        TerminalPrintLine32("  cpuinfo                         CPUID vendor/signature/features", PromColor);
        TerminalPrintLine32("  cpuidx <leaf> [subleaf]        Execute raw CPUID", PromColor);
        TerminalPrintLine32("  cpuidmax                        Show CPUID limits", PromColor);
        TerminalPrintLine32("  cpuidbits                       List feature bits", PromColor);
        TerminalPrintLine32("  flags                           Read RFLAGS bit fields", PromColor);
        TerminalPrintLine32("  regs                            Dump general registers", PromColor);
        TerminalPrintLine32("  segs                            Dump segment selectors", PromColor);
        TerminalPrintLine32("  rsp / rbp / rip                 Inspect execution pointers", PromColor);
        TerminalPrintLine32("  rdtscp                          Read TSC + TSC_AUX", PromColor);
        TerminalPrintLine32("  xgetbv [index]                  Read XCR", PromColor);
        TerminalPrintLine32("  crinfo                          Dump CR0/CR2/CR3/CR4", PromColor);
        TerminalPrintLine32("  stack [qwords]                  Dump memory below current RSP", PromColor);
        TerminalPrintLine32("  tscbench [loops]                Measure PAUSE with TSC", PromColor);
        TerminalPrintLine32("  pause                           Execute CPU PAUSE", PromColor);
        TerminalPrintLine32("  hypervisor                      Detect CPUID hypervisor", PromColor);
        TerminalPrintLine32("  tscinfo                         Inspect TSC frequency leaves", PromColor);
        TerminalPrintLine32("  cacheinfo                       Decode deterministic cache leaf", PromColor);
        TerminalPrintLine32("  fpuinfo / simdinfo              Inspect FPU/SIMD state and features", PromColor);
        TerminalPrintLine32("  cmos <reg> / rtc                Read RTC/CMOS bytes", PromColor);
        TerminalPrintLine32("  pci <b> <s> <f> <off>           Read PCI config DWORD", PromColor);
        TerminalPrintLine32("  pciscan [bus]                   Scan PCI bus", PromColor);
        TerminalPrintLine32("  pcidump <b> <s> <f>             Dump 256-byte PCI config space", PromColor);
        TerminalPrintLine32("  pit / pic                       Inspect timer/PIC state", PromColor);
        TerminalPrintLine32("  iodly                           Hit legacy I/O delay port", PromColor);
        TerminalPrintLine32("  e9 <text...>                    Write QEMU/Bochs debug port", PromColor);
        TerminalPrintLine32("  portwatch <port> [reads]        Sample an I/O port", PromColor);
        TerminalPrintLine32("  memscan <addr> <bytes> <byte>   Scan raw memory", PromColor);
        TerminalPrintLine32("  memfill <addr> <bytes> <byte>   Write raw memory", PromColor);
        TerminalPrintLine32("  memxor <addr> <bytes> <byte>    XOR raw memory", PromColor);
        TerminalPrintLine32("  bits <value>                    Binary bit view", PromColor);
        TerminalPrintLine32("  endian <value>                  Byte-swap 64-bit value", PromColor);
        TerminalPrintLine32("  align <addr> [alignment]        Alignment math", PromColor);
        TerminalPrintLine32("  ioports                         Print common legacy I/O map", PromColor);
        TerminalPrintLine32("  pageinfo                        Inspect paging control bits", PromColor);
        TerminalPrintLine32("  dbgregs                         Dump hardware debug registers", PromColor);
        TerminalPrintLine32("  gdtidt                          Read GDTR and IDTR", PromColor);
        TerminalPrintLine32("  serial <hexbytes>               Send raw bytes to COM1", PromColor);
        TerminalPrintLine32("  WARNING: memfill/memxor/memscan and port writes can crash hardware.", 0x00FF8800);
        return;
    }

    TerminalWrite32("help: unknown category '", 0x00FF0000);
    TerminalWrite32(Category, 0x00FF0000);
    TerminalPrintLine32("'", 0x00FF0000);
    TerminalPrintLine32("Categories: system, filesystem, memory, iso, registry, sound, developer, lowlevel", PromColor);
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

static void RegFormatHex(char *Out, uint32_t V)
{
    static const char *H = "0123456789ABCDEF";
    Out[0] = '0';
    Out[1] = 'x';
    for (int i = 0; i < 8; i++)
    {
        Out[2 + i] = H[(V >> ((7 - i) * 4U)) & 0xFU];
    }
    Out[10] = '\0';
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

    if (DiskFsIsMounted())
    {
        char Hex[16];
        char SavedCwd[64];
        DiskFsPwd(SavedCwd, sizeof(SavedCwd));
        DiskFsMkdirAll("/REG");
        DiskFsMkdirAll("/REG/color");
        DiskFsChdirPath(SavedCwd);
        RegFormatHex(Hex, ColorFG);
        DiskFsWriteInPath("/REG/color", "fg.v", Hex, 10);
        RegFormatHex(Hex, ColorBG);
        DiskFsWriteInPath("/REG/color", "bg.v", Hex, 10);
        TerminalWrite32("regfill: saved /REG/color/fg.v and /REG/color/bg.v (persists across reboot)\n", PromColor);
    }
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

typedef struct
{
    uint32_t Index;
} CmdListContext;

#define LS_COLOR_DIRECTORY 0x0040A0FFU
#define LS_COLOR_ELF       0x0037D67AU
#define LS_COLOR_FILE      0xFFFFFFFFU

static bool CmdNameIsElf(const char *Name)
{
    uint32_t Len = (uint32_t)strlen(Name);
    const char *Ext;

    if (Len < 4U) return false;
    Ext = Name + Len - 4U;
    return Ext[0] == '.' &&
           (Ext[1] == 'E' || Ext[1] == 'e') &&
           (Ext[2] == 'L' || Ext[2] == 'l') &&
           (Ext[3] == 'F' || Ext[3] == 'f');
}

static uint32_t LsColumnPos;

static bool CmdListVisitor(const FAT32DirectoryEntry *Entry,
                           const char *ShortName, void *Context)
{
    uint32_t Color;
    uint32_t Len;

    (void)Context;
    if ((Entry->Attributes & FAT32_ATTRIBUTE_DIRECTORY) != 0)
    {
        Color = LS_COLOR_DIRECTORY;
    }
    else if (CmdNameIsElf(ShortName))
    {
        Color = LS_COLOR_ELF;
    }
    else
    {
        Color = LS_COLOR_FILE;
    }

    Len = (uint32_t)strlen(ShortName);
    if (LsColumnPos + Len + 4U >= 80U)
    {
        TerminalPutChar32('\n', PromColor);
        LsColumnPos = 0U;
    }
    TerminalWrite32(ShortName, Color);
    LsColumnPos += Len;
    for (uint32_t i = 0; i < 4U; i++)
    {
        TerminalPutChar32(' ', PromColor);
    }
    LsColumnPos += 4U;
    return true;
}

void CmdLs(void)
{
    const FAT32Directory *Cwd;

    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("ls: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    Cwd = DiskFsCwd();
    if (Cwd == NULL || !FAT32DirectoryForEach(Cwd, CmdListVisitor, NULL))
    {
        TerminalPrintLine32("ls: cannot read directory", 0x00FF0000);
        return;
    }
    if (LsColumnPos != 0U)
    {
        TerminalPutChar32('\n', PromColor);
        LsColumnPos = 0U;
    }
}

void CmdTouch(const char *Path)
{
    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("touch: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    if (!DiskFsTouch(Path)) TerminalPrintLine32("touch: cannot create file", 0x00FF0000);
}

void CmdCat(const char *Path, const char *LineArg)
{
    FAT32DirectoryEntry Entry;
    const FAT32Directory *Cwd;
    uint8_t *Buffer;
    uint32_t BytesRead = 0;
    uint64_t LineVal = 0;

    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("cat: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    Cwd = DiskFsCwd();
    if (Cwd == NULL || !FAT32DirectoryFind(Cwd, Path, &Entry))
    {
        TerminalPrintLine32("cat: file not found", 0x00FF0000);
        return;
    }

    Buffer = (uint8_t *)kmalloc(Entry.FileSize + 1U);
    if (Buffer == NULL)
    {
        TerminalPrintLine32("cat: out of memory", 0x00FF0000);
        return;
    }
    if (!DiskFsRead(Path, Buffer, Entry.FileSize, &BytesRead))
    {
        TerminalPrintLine32("cat: read failed", 0x00FF0000);
        return;
    }
    Buffer[BytesRead] = '\0';

    if (LineArg != NULL)
    {
        if (!ParseNumber(LineArg, &LineVal) || LineVal == 0)
        {
            TerminalPrintLine32("cat: invalid line number", 0x00FF0000);
            kfree(Buffer);
            return;
        }
        /* Print only the requested 1-based line. */
        const char *S = (const char *)Buffer;
        const char *LineStart = NULL;
        uint64_t Cur = 1;
        while (*S != '\0')
        {
            if (Cur == LineVal)
            {
                LineStart = S;
                break;
            }
            while (*S != '\0' && *S != '\n')
            {
                S++;
            }
            if (*S == '\n')
            {
                S++;
            }
            Cur++;
        }
        if (LineStart != NULL)
        {
            while (*LineStart != '\0' && *LineStart != '\n')
            {
                TerminalPutChar32(*LineStart, PromColor);
                LineStart++;
            }
        }
        TerminalPutChar32('\n', PromColor);
        kfree(Buffer);
        return;
    }

    HighlightPrintFile(Path, (const char *)Buffer);
    TerminalPutChar32('\n', PromColor);
}

void CmdRm(const char *Path)
{
    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("rm: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    if (!DiskFsRemove(Path)) TerminalPrintLine32("rm: file not found", 0x00FF0000);
}

void CmdCd(const char *Path)
{
    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("cd: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    if (Path == NULL || Path[0] == '\0' || strcmp(Path, "~") == 0)
    {
        DiskFsCdRoot();
        return;
    }
    if (!DiskFsCd(Path))
    {
        TerminalWrite32("cd: no such directory: ", 0x00FF0000);
        TerminalPrintLine32(Path, 0x00FF0000);
    }
}

void CmdMkdir(const char *Path)
{
    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("mkdir: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    if (!DiskFsMkdir(Path))
    {
        TerminalWrite32("mkdir: cannot create directory: ", 0x00FF0000);
        TerminalPrintLine32(Path, 0x00FF0000);
    }
}

void CmdRmdir(const char *Path)
{
    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("rmdir: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    if (!DiskFsRemove(Path))
    {
        TerminalWrite32("rmdir: cannot remove directory: ", 0x00FF0000);
        TerminalPrintLine32(Path, 0x00FF0000);
    }
}

void CmdPwd(void)
{
    char Path[256];

    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("pwd: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    DiskFsPwd(Path, sizeof(Path));
    TerminalPrintLine32(Path, PromColor);
}

void CmdWrite(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 2U)
    {
        TerminalPrintLine32("Usage: write <file> <text>", 0x00FF0000);
        return;
    }

    /* Total bytes needed: every argument plus a separating space. */
    uint32_t Total = 0;
    for (uint32_t Argument = 1; Argument < Command->ArgCount; Argument++)
    {
        Total += (uint32_t)strlen(Command->Args[Argument]);
        if (Argument + 1U < Command->ArgCount) Total += 1U;
    }

    char *Text = (char *)kmalloc((uint64_t)Total + 1U);
    if (Text == NULL)
    {
        TerminalPrintLine32("write: out of memory", 0x00FF0000);
        return;
    }

    uint32_t Position = 0;
    for (uint32_t Argument = 1; Argument < Command->ArgCount; Argument++)
    {
        const char *Part = Command->Args[Argument];
        for (uint32_t Index = 0; Part[Index] != '\0'; Index++)
            Text[Position++] = Part[Index];
        if (Argument + 1U < Command->ArgCount)
            Text[Position++] = ' ';
    }
    Text[Position] = '\0';

    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        kfree(Text);
        TerminalPrintLine32("write: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    if (!DiskFsWrite(Command->Args[0], Text, Position))
    {
        kfree(Text);
        TerminalPrintLine32("write: cannot write file", 0x00FF0000);
        return;
    }
    kfree(Text);
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
             strcmp(CommandName, "write") == 0 || strcmp(CommandName, "rm") == 0 || strcmp(CommandName, "edit") == 0 ||
             strcmp(CommandName, "mkdir") == 0 || strcmp(CommandName, "cd") == 0 || strcmp(CommandName, "pwd") == 0 ||
             strcmp(CommandName, "rmdir") == 0)
    {
        CmdSufLine("   shell/diskfs.c & fs/fat32 -> FAT32 disk filesystem engine");
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
        CmdSufLine("   shell/diskfs.c & fs/fat32/directory/directory.c -> Recursive FAT32 tree walker");
    }
    else if (strcmp(CommandName, "panic") == 0)
    {
        CmdSufLine("   kernel/panic/panic.c -> PanicKernel() software kernel panic");
    }
    else if (strcmp(CommandName, "where") == 0)
    {
        CmdSufLine("   shell/commands/cmd.c -> CmdShowAddr() Dynamic symbol address tracer");
    }
    else if (strcmp(CommandName, "regfill") == 0 || strcmp(CommandName, "RegFill") == 0)
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

void CmdNcc(const PARSED_COMMAND *Command)
{
    const char *Src = NULL;
    const char *Out = NULL;
    char Source[32768];
    uint8_t *Image = NULL;
    uint32_t Bytes = 0;
    uint32_t ImageSize = 0;
    bool ObjOnly = false;

    if (Command == NULL || Command->ArgCount == 0U)
    {
        TerminalPrintLine32("Usage: ncc [-o <out>] [--obj] <file.c>", 0x00FF0000);
        return;
    }
    for (uint32_t i = 0; i < Command->ArgCount; i++)
    {
        if (strcasecmp(Command->Args[i], "-o") == 0)
        {
            if (i + 1U >= Command->ArgCount)
            {
                TerminalPrintLine32("ncc: -o needs a path", 0x00FF0000);
                return;
            }
            Out = Command->Args[i + 1U];
            i++;
        }
        else if (strcasecmp(Command->Args[i], "--obj") == 0)
        {
            ObjOnly = true;
        }
        else if (Src == NULL)
        {
            Src = Command->Args[i];
        }
    }
    if (Src == NULL)
    {
        TerminalPrintLine32("Usage: ncc [-o <out>] [--obj] <file.c>", 0x00FF0000);
        return;
    }
    if (!DiskFsIsMounted())
    {
        TerminalPrintLine32("ncc: no disk mounted", 0x00FF5C77);
        return;
    }
    if (!DiskFsRead(Src, Source, sizeof(Source) - 1U, &Bytes))
    {
        TerminalPrintLine32("ncc: cannot read file", 0x00FF5C77);
        return;
    }
    Source[Bytes] = '\0';
    TerminalWrite32("ncc: compiling ", PromColor);
    TerminalPrintLine32(Src, PromColor);
    if (ObjOnly)
    {
        if (!NccCompileObject(Src, Source, Bytes, &Image, &ImageSize))
        {
            TerminalPrintLine32("ncc: compile failed", 0x00FF5C77);
            return;
        }
    }
    else if (!NccCompile(Src, Source, Bytes, &Image, &ImageSize))
    {
        TerminalPrintLine32("ncc: compile failed", 0x00FF5C77);
        return;
    }
    if (Out != NULL)
    {
        char Dir[64];
        uint32_t i2 = 0, last = 0;
        while (Out[i2] != '\0')
        {
            if (Out[i2] == '/')
            {
                last = i2;
            }
            i2++;
        }
        if (last > 0)
        {
            uint32_t dlen = (last < sizeof(Dir) - 1U) ? last : (sizeof(Dir) - 1U);
            char SavedCwd[64];
            memcpy(Dir, Out, dlen);
            Dir[dlen] = '\0';
            DiskFsPwd(SavedCwd, sizeof(SavedCwd));
            DiskFsMkdirAll(Dir);
            DiskFsChdirPath(SavedCwd);
        }
        if (!DiskFsWrite(Out, Image, ImageSize))
        {
            TerminalPrintLine32("ncc: cannot write output", 0x00FF5C77);
            kfree(Image);
            return;
        }
        TerminalWrite32("ncc: wrote ", PromColor);
        TerminalPrintLine32(Out, PromColor);
    }
    else if (!ObjOnly)
    {
        char ExeName[13];
        SelfHostExeName(Src, ExeName);
        if (!DiskFsWrite(ExeName, Image, ImageSize))
        {
            TerminalPrintLine32("ncc: cannot write output", 0x00FF5C77);
            kfree(Image);
            return;
        }
        TerminalWrite32("ncc: wrote ", PromColor);
        TerminalPrintLine32(ExeName, PromColor);
    }
    else
    {
        char ObjName[16];
        uint32_t b = 0;
        while (Src[b] != '\0' && Src[b] != '.' && b < 8U)
        {
            ObjName[b] = Src[b];
            b++;
        }
        strcpy(ObjName + b, ".OBJ");
        if (!DiskFsWrite(ObjName, Image, ImageSize))
        {
            TerminalPrintLine32("ncc: cannot write output", 0x00FF5C77);
            kfree(Image);
            return;
        }
        TerminalWrite32("ncc: wrote ", PromColor);
        TerminalPrintLine32(ObjName, PromColor);
    }
    kfree(Image);
}

/* ---- n64a -lc / --obj support -------------------------------------------------
 * The kernel symbol vault is the set of "known APIs" we can link against.
 * External references that are neither in the vault nor defined inside the
 * object itself are unknown APIs. */

static uint32_t SH_Rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Collect the names of external references that are NOT known APIs and NOT
 * symbols defined inside this object. Returns the count (capped at Max). */
static uint32_t SH_CollectUnknownApis(const uint8_t *Obj, uint32_t Size,
                                      char (*Out)[N64O_NAME_MAX], uint32_t Max)
{
    uint32_t text, data, nsym, nfix, symOff, relOff, i, cnt = 0;

    if (Obj == NULL || Size < N64O_HDRSZ)
        return 0;
    text = SH_Rd32(Obj + 16);
    data = SH_Rd32(Obj + 20);
    nsym = SH_Rd32(Obj + 24);
    nfix = SH_Rd32(Obj + 28);
    symOff = N64O_HDRSZ + text + data;
    relOff = symOff + nsym * (uint32_t)sizeof(N64ObjSymbol);

    for (i = 0; i < nfix; i++)
    {
        const char *name = (const char *)(Obj + relOff +
                                          i * (uint32_t)sizeof(N64ObjReloc) + 12);
        uint32_t j;
        bool dup = false;

        if (name[0] == '\0')
            continue;
        if (NccVaultHasSymbol(name))
            continue;
        /* defined locally inside this object? */
        for (j = 0; j < nsym; j++)
        {
            const char *ls = (const char *)(Obj + symOff +
                                           j * (uint32_t)sizeof(N64ObjSymbol) + 4);
            if (ls[0] != '\0' && strcmp(ls, name) == 0)
                break;
        }
        if (j < nsym)
            continue;
        for (j = 0; j < cnt; j++)
            if (strcmp(Out[j], name) == 0) { dup = true; break; }
        if (!dup && cnt < Max)
        {
            strncpy(Out[cnt], name, N64O_NAME_MAX - 1U);
            Out[cnt][N64O_NAME_MAX - 1U] = '\0';
            cnt++;
        }
    }
    return cnt;
}

/* n64a <file.asm> [-o <out.obj>] [--obj] [-lc]
 *   --obj : emit a .obj and do NOT auto-link to .exe
 *   -lc   : link-check external APIs against the kernel symbol vault
 *           (known APIs link; unknown APIs warn or error depending on --obj) */
void CmdN64a(const PARSED_COMMAND *Command)
{
    const char *Src = NULL;
    const char *Out = NULL;
    char DefaultOut[16];
    char ExeName[16];
    uint32_t b;
    bool ObjOnly = false;
    bool LinkCheck = false;

    if (Command == NULL || Command->ArgCount == 0U)
    {
        TerminalPrintLine32("Usage: n64a <file.asm> [-o <out.obj>] [--obj] [-lc]",
                            0x00FF0000);
        return;
    }
    for (uint32_t i = 0; i < Command->ArgCount; i++)
    {
        if (strcasecmp(Command->Args[i], "-o") == 0)
        {
            if (i + 1U >= Command->ArgCount)
            {
                TerminalPrintLine32("n64a: -o needs a path", 0x00FF0000);
                return;
            }
            Out = Command->Args[i + 1U];
            i++;
        }
        else if (strcasecmp(Command->Args[i], "--obj") == 0)
        {
            ObjOnly = true;
        }
        else if (strcasecmp(Command->Args[i], "-lc") == 0)
        {
            LinkCheck = true;
        }
        else if (Src == NULL)
        {
            Src = Command->Args[i];
        }
    }
    if (Src == NULL)
    {
        TerminalPrintLine32("Usage: n64a <file.asm> [-o <out.obj>] [--obj] [-lc]",
                            0x00FF0000);
        return;
    }
    if (Out == NULL)
    {
        b = 0;
        while (Src[b] != '\0' && Src[b] != '.' && b < 8U)
        {
            DefaultOut[b] = Src[b];
            b++;
        }
        strcpy(DefaultOut + b, ".OBJ");
        Out = DefaultOut;
    }

    if (!DiskFsIsMounted())
    {
        TerminalPrintLine32("n64a: no disk mounted", 0xFF5C5CFF);
        return;
    }
    uint8_t *Obj = NULL;
    uint32_t Sz = 0;
    if (!SelfHostAssembleObject(Src, &Obj, &Sz))
    {
        kfree(Obj);
        return;
    }

    char Unk[64][N64O_NAME_MAX];
    uint32_t UnkCount = 0;
    if (LinkCheck)
        UnkCount = SH_CollectUnknownApis(Obj, Sz, Unk, 64U);

    if (LinkCheck && UnkCount > 0U)
    {
        if (ObjOnly)
        {
            for (uint32_t i = 0; i < UnkCount; i++)
            {
                TerminalWrite32("WARNING: ", 0xFFFFAA00);
                TerminalWrite32(Unk[i], 0xFFFFAA00);
                TerminalPrintLine32(" is unknown api, I will expect link with other .obj",
                                    0xFFFFAA00);
            }
            if (DiskFsWrite(Out, Obj, Sz))
            {
                TerminalWrite32("n64a: wrote ", 0xFF88FF88);
                TerminalPrintLine32(Out, 0xFF88FF88);
            }
            kfree(Obj);
            return;
        }
        for (uint32_t i = 0; i < UnkCount; i++)
        {
            TerminalWrite32("ERROR: ", 0x00FF0000);
            TerminalWrite32(Unk[i], 0x00FF0000);
            TerminalPrintLine32(" is unknown, if this is from a C file, please add --obj then link them by nld",
                                0x00FF0000);
        }
        kfree(Obj);
        return;
    }

    /* write the object */
    if (!DiskFsWrite(Out, Obj, Sz))
    {
        TerminalPrintLine32("n64a: cannot write output", 0xFF5C5CFF);
        kfree(Obj);
        return;
    }
    TerminalWrite32("n64a: wrote ", 0xFF88FF88);
    TerminalPrintLine32(Out, 0xFF88FF88);
    kfree(Obj);

    if (ObjOnly)
        return;

    /* auto-link into a .exe (default behaviour when not --obj) */
    b = 0;
    while (Out[b] != '\0' && Out[b] != '.' && b < 8U)
    {
        ExeName[b] = Out[b];
        b++;
    }
    strcpy(ExeName + b, ".EXE");
    {
        const char *Objects[1];
        Objects[0] = Out;
        if (!SelfHostLinkFiles(Objects, 1U, ExeName))
        {
            return;
        }
    }
    TerminalWrite32("n64a: linked ", 0xFF88FF88);
    TerminalPrintLine32(ExeName, 0xFF88FF88);
}

/* nld <out.exe> <a.obj> [b.obj ...] — links n64a/NCC objects. */
void CmdNld(const PARSED_COMMAND *Command)
{
    const char *Objects[16];
    const char *Out = NULL;
    uint32_t Count = 0;

    if (Command == NULL || Command->ArgCount < 2U)
    {
        TerminalPrintLine32("Usage: nld <out.exe> <a.obj> [b.obj ...]",
                            0x00FF0000);
        return;
    }
    for (uint32_t i = 0; i < Command->ArgCount; i++)
    {
        if (strcasecmp(Command->Args[i], "-o") == 0)
        {
            if (i + 1U >= Command->ArgCount)
            {
                TerminalPrintLine32("nld: -o needs a path", 0x00FF0000);
                return;
            }
            Out = Command->Args[i + 1U];
            i++;
        }
        else if (Out == NULL && Count == 0U)
        {
            Out = Command->Args[i];
        }
        else if (Count < 16U)
        {
            Objects[Count++] = Command->Args[i];
        }
    }
    if (Out == NULL || Count == 0U)
    {
        TerminalPrintLine32("Usage: nld <out.exe> <a.obj> [b.obj ...]",
                            0x00FF0000);
        return;
    }
    SelfHostLinkFiles(Objects, Count, Out);
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

static void CmdPrintHexByte(uint8_t Byte)
{
    static const char HEX[] = "0123456789ABCDEF";
    char Out[3];

    Out[0] = HEX[(Byte >> 4) & 0x0F];
    Out[1] = HEX[Byte & 0x0F];
    Out[2] = '\0';
    TerminalWrite32(Out, PromColor);
}

void CmdDiskRead(const PARSED_COMMAND *Command)
{
    uint8_t buf[512];

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

    memset(buf, 0, sizeof(buf));
    if (!IDEReadSector((uint32_t)lba, buf))
    {
        TerminalPrintLine32("diskread: sector read failed", 0xFFFF0000);
        return;
    }

    TerminalWrite32("Sector LBA ", 0x0057DB92);
    TerminalPrintHex64Compact(lba);
    TerminalPrintLine32(" - full 512 bytes:", 0x0057DB92);
    for (uint32_t i = 0; i < 512; i += 16)
    {
        char Off[6];

        Off[0] = "0123456789ABCDEF"[(i >> 12) & 0xF];
        Off[1] = "0123456789ABCDEF"[(i >> 8) & 0xF];
        Off[2] = "0123456789ABCDEF"[(i >> 4) & 0xF];
        Off[3] = "0123456789ABCDEF"[i & 0xF];
        Off[4] = ':';
        Off[5] = '\0';
        TerminalWrite32(Off, 0x0057DB92);
        TerminalWrite32(" ", PromColor);
        for (uint32_t j = 0; j < 16; j++)
        {
            CmdPrintHexByte(buf[i + j]);
            TerminalPutChar32(' ', PromColor);
        }
        TerminalWrite32("|", 0x00314A70);
        for (uint32_t j = 0; j < 16; j++)
        {
            uint8_t c = buf[i + j];
            TerminalPutChar32((c >= 32 && c <= 126) ? (char)c : '.', PromColor);
        }
        TerminalWrite32("|", 0x00314A70);
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
    TerminalWrite32("  Filesystem   : ", 0x0057DB92); TerminalPrintLine32("FAT32 (disk) + ISO9660 (CD)", PromColor);
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

typedef struct
{
    const FAT32Directory *Cwd;
    uint32_t Depth;
} CmdTreeContext;

static bool CmdTreeVisitor(const FAT32DirectoryEntry *Entry,
                           const char *ShortName, void *Context)
{
    CmdTreeContext *Ctx = (CmdTreeContext *)Context;

    if (strcmp(ShortName, ".") == 0 || strcmp(ShortName, "..") == 0)
    {
        return true;
    }
    for (uint32_t Index = 0; Index < Ctx->Depth; Index++)
    {
        TerminalWrite32("    ", PromColor);
    }
    TerminalWrite32("└── ", 0x005AA9FF);
    TerminalWrite32(ShortName, PromColor);
    if ((Entry->Attributes & FAT32_ATTRIBUTE_DIRECTORY) != 0)
    {
        TerminalWrite32("/\n", PromColor);
        if (Ctx->Depth < 4U)
        {
            FAT32Directory Child;
            CmdTreeContext ChildCtx;

            if (FAT32DirectoryOpen(&Child, Ctx->Cwd->Boot,
                                   Ctx->Cwd->PartitionStartLBA,
                                   FAT32DirectoryEntryFirstCluster(Entry)))
            {
                ChildCtx.Cwd = &Child;
                ChildCtx.Depth = Ctx->Depth + 1U;
                FAT32DirectoryForEach(&Child, CmdTreeVisitor, &ChildCtx);
            }
        }
    }
    else
    {
        TerminalWrite32("  ", PromColor);
        TerminalPrintHex64Compact(Entry->FileSize);
        TerminalPutChar32('\n', PromColor);
    }
    return true;
}

void CmdTree(void)
{
    const FAT32Directory *Cwd;

    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("tree: no FAT32 disk mounted", 0x00FF0000);
        return;
    }
    Cwd = DiskFsCwd();
    if (Cwd == NULL)
    {
        return;
    }
    TerminalPrintLine32(".", 0x0057DB92);
    {
        CmdTreeContext Root;
        Root.Cwd = Cwd;
        Root.Depth = 0;
        FAT32DirectoryForEach(Cwd, CmdTreeVisitor, &Root);
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

void CmdIsoCopy(const char *IsoPath, const char *DiskPath)
{
    ISODirRecord FileRec;
    if (!ISOFindPath(IsoPath, &FileRec))
    {
        TerminalPrintLine32("isocopy: Source file not found on ISO", 0xFFFF0000);
        return;
    }

    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("isocopy: no FAT32 disk mounted", 0xFFFF0000);
        return;
    }

    if (FileRec.DataLength == 0)
    {
        if (DiskFsTouch(DiskPath))
            TerminalPrintLine32("isocopy: Successfully copied to disk!", 0x0057DB92);
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

    if (DiskFsWrite(DiskPath, buf, bytesRead))
    {
        TerminalPrintLine32("isocopy: Successfully copied to disk!", 0x0057DB92);
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

/* ----------------------------- cp ----------------------------- */

static void CpStripTrailingSlash(char *p)
{
    uint32_t n = strlen(p);
    while (n > 1U && p[n - 1U] == '/')
    {
        p[--n] = '\0';
    }
}

/* Split a path into directory and final FAT32 8.3-style component. */
static void CpSplit(const char *path, char *dir, uint32_t dirN,
                    char *name, uint32_t nameN)
{
    uint32_t len = strlen(path);
    uint32_t last = 0U;

    for (uint32_t i = 0U; i < len; i++)
    {
        if (path[i] == '/') last = i;
    }

    if (len > 0U && path[last] == '/')
    {
        if (last == 0U)
        {
            strcpy(dir, "/");
            strncpy(name, path + 1U, nameN - 1U);
        }
        else
        {
            uint32_t copy = last < dirN ? last : dirN - 1U;
            strncpy(dir, path, copy);
            dir[copy] = '\0';
            strncpy(name, path + last + 1U, nameN - 1U);
        }
    }
    else
    {
        strcpy(dir, ".");
        strncpy(name, path, nameN - 1U);
    }
    dir[dirN - 1U] = '\0';
    name[nameN - 1U] = '\0';
}

/* True when path names an existing directory. */
static bool CpIsDir(const char *path)
{
    FAT32DirectoryEntry e;
    char dir[DISKFS_PATH_MAX];
    char name[DISKFS_PATH_MAX];

    if (path == NULL || path[0] == '\0') return false;
    uint32_t len = strlen(path);
    if (path[len - 1U] == '/') return true;

    CpSplit(path, dir, sizeof(dir), name, sizeof(name));
    if (name[0] == '\0') return true;
    if (!DiskFsFindInPath(dir, name, &e)) return false;
    return (e.Attributes & FAT32_ATTRIBUTE_DIRECTORY) != 0U;
}

static bool CpMakeAbsolute(const char *path, char *out, uint32_t outN)
{
    char cwd[DISKFS_PATH_MAX];
    uint32_t pathLen = strlen(path);
    uint32_t cwdLen;

    if (path == NULL || out == NULL || outN == 0U || pathLen >= outN)
        return false;

    if (path[0] == '/')
    {
        strcpy(out, path);
        return true;
    }

    DiskFsPwd(cwd, sizeof(cwd));
    cwdLen = strlen(cwd);

    if (strcmp(cwd, "/") == 0)
    {
        if (pathLen + 2U > outN) return false;
        out[0] = '/';
        strcpy(out + 1U, path);
        return true;
    }

    if (cwdLen + 1U + pathLen + 1U > outN)
        return false;

    strcpy(out, cwd);
    out[cwdLen] = '/';
    strcpy(out + cwdLen + 1U, path);
    return true;
}

static bool CpSameOrChild(const char *src, const char *dst)
{
    uint32_t n = strlen(src);
    uint32_t dstLen = strlen(dst);

    if (strcmp(src, dst) == 0) return true;
    if (n == 0U || dstLen <= n || dst[n] != '/') return false;
    for (uint32_t i = 0U; i < n; i++)
    {
        if (src[i] != dst[i]) return false;
    }
    return true;
}

static bool CpCopyFile(const char *src, const char *dst)
{
    char sDir[DISKFS_PATH_MAX], sName[DISKFS_PATH_MAX];
    char dDir[DISKFS_PATH_MAX], dName[DISKFS_PATH_MAX];
    char finalDst[DISKFS_PATH_MAX];
    FAT32DirectoryEntry sourceEntry;
    uint32_t size = 0U, read = 0U;
    uint8_t *buf;

    CpSplit(src, sDir, sizeof(sDir), sName, sizeof(sName));
    if (!DiskFsFindInPath(sDir, sName, &sourceEntry) ||
        (sourceEntry.Attributes & FAT32_ATTRIBUTE_DIRECTORY) != 0U ||
        !DiskFsFileSizeInPath(sDir, sName, &size))
    {
        TerminalPrintLine32("cp: source is not a regular file", 0xFFFF0000U);
        return false;
    }

    buf = (uint8_t *)kmalloc(size ? size : 1U);
    if (buf == NULL)
    {
        TerminalPrintLine32("cp: out of memory", 0xFFFF0000U);
        return false;
    }

    if (!DiskFsReadInPath(sDir, sName, buf, size, &read))
    {
        TerminalPrintLine32("cp: read failed", 0xFFFF0000U);
        kfree(buf);
        return false;
    }

    if (CpIsDir(dst))
    {
        uint32_t need = strlen(dst) + strlen(sName) + 2U;
        if (need >= sizeof(finalDst))
        {
            TerminalPrintLine32("cp: destination path too long", 0xFFFF0000U);
            kfree(buf);
            return false;
        }
        strcpy(finalDst, dst);
        CpStripTrailingSlash(finalDst);
        strcat(finalDst, "/");
        strcat(finalDst, sName);
    }
    else
    {
        if (strlen(dst) >= sizeof(finalDst))
        {
            TerminalPrintLine32("cp: destination path too long", 0xFFFF0000U);
            kfree(buf);
            return false;
        }
        strcpy(finalDst, dst);
    }

    CpSplit(finalDst, dDir, sizeof(dDir), dName, sizeof(dName));
    if (dDir[0] != '\0' && strcmp(dDir, ".") != 0 && !DiskFsMkdirAll(dDir))
    {
        TerminalPrintLine32("cp: cannot create destination directory", 0xFFFF0000U);
        kfree(buf);
        return false;
    }

    bool ok = DiskFsWriteInPath(dDir, dName, buf, read);
    kfree(buf);
    if (!ok)
    {
        TerminalPrintLine32("cp: write failed", 0xFFFF0000U);
        return false;
    }
    return true;
}

static bool CpCopyTree(const char *srcDir, const char *dstDir)
{
    char saved[DISKFS_PATH_MAX];
    char list[4096];
    uint32_t off = 0U;

    if (!DiskFsMkdirAll(dstDir))
    {
        TerminalPrintLine32("cp: cannot create destination directory", 0xFFFF0000U);
        return false;
    }

    DiskFsPwd(saved, sizeof(saved));
    if (!DiskFsChdirPath(srcDir))
    {
        TerminalPrintLine32("cp: cannot open source directory", 0xFFFF0000U);
        return false;
    }

    list[0] = '\0';
    DiskFsListNames(list, sizeof(list));

    while (list[off] != '\0')
    {
        char name[64];
        uint32_t n = 0U;

        while (list[off] != '\0' && list[off] != '\n' && n < sizeof(name) - 1U)
            name[n++] = list[off++];
        if (list[off] == '\n') off++;
        name[n] = '\0';

        if (name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        if (DiskFsIsDirectory(name))
        {
            char childSrc[DISKFS_PATH_MAX];
            char childDst[DISKFS_PATH_MAX];

            if (strlen(srcDir) + strlen(name) + 2U >= sizeof(childSrc) ||
                strlen(dstDir) + strlen(name) + 2U >= sizeof(childDst))
            {
                TerminalPrintLine32("cp: path too long", 0xFFFF0000U);
                DiskFsChdirPath(saved);
                return false;
            }

            strcpy(childSrc, srcDir);
            CpStripTrailingSlash(childSrc);
            strcat(childSrc, "/");
            strcat(childSrc, name);

            strcpy(childDst, dstDir);
            CpStripTrailingSlash(childDst);
            strcat(childDst, "/");
            strcat(childDst, name);

            if (!CpCopyTree(childSrc, childDst))
            {
                DiskFsChdirPath(saved);
                return false;
            }
        }
        else
        {
            char childSrc[DISKFS_PATH_MAX];
            char childDst[DISKFS_PATH_MAX];

            if (strlen(srcDir) + strlen(name) + 2U >= sizeof(childSrc) ||
                strlen(dstDir) + strlen(name) + 2U >= sizeof(childDst))
            {
                TerminalPrintLine32("cp: path too long", 0xFFFF0000U);
                DiskFsChdirPath(saved);
                return false;
            }

            strcpy(childSrc, srcDir);
            CpStripTrailingSlash(childSrc);
            strcat(childSrc, "/");
            strcat(childSrc, name);

            strcpy(childDst, dstDir);
            CpStripTrailingSlash(childDst);
            strcat(childDst, "/");
            strcat(childDst, name);

            if (!CpCopyFile(childSrc, childDst))
            {
                DiskFsChdirPath(saved);
                return false;
            }
        }
    }

    DiskFsChdirPath(saved);
    return true;
}

void CmdCp(const char *Src, const char *Dst)
{
    char srcNorm[DISKFS_PATH_MAX];
    char dstNorm[DISKFS_PATH_MAX];

    if (Src == NULL || Dst == NULL || Src[0] == '\0' || Dst[0] == '\0')
    {
        TerminalPrintLine32("Usage: cp <source> <dest>", 0x00FF0000U);
        return;
    }
    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        TerminalPrintLine32("cp: no disk mounted", 0xFFFF0000U);
        return;
    }
    if (strlen(Src) >= sizeof(srcNorm) || strlen(Dst) >= sizeof(dstNorm))
    {
        TerminalPrintLine32("cp: path too long", 0xFFFF0000U);
        return;
    }

    if (!CpMakeAbsolute(Src, srcNorm, sizeof(srcNorm)) ||
        !CpMakeAbsolute(Dst, dstNorm, sizeof(dstNorm)))
    {
        TerminalPrintLine32("cp: cannot resolve path", 0xFFFF0000U);
        return;
    }
    CpStripTrailingSlash(srcNorm);
    CpStripTrailingSlash(dstNorm);

    if (strcmp(srcNorm, dstNorm) == 0)
    {
        TerminalPrintLine32("cp: source and destination are the same", 0xFFFF0000U);
        return;
    }

    if (CpIsDir(srcNorm))
    {
        char finalDir[DISKFS_PATH_MAX];
        char base[DISKFS_PATH_MAX];
        const char *slash = srcNorm;

        for (const char *p = srcNorm; *p != '\0'; p++)
            if (*p == '/') slash = p + 1;

        if (*slash == '\0') slash = srcNorm;

        if (CpIsDir(dstNorm))
        {
            if (CpSameOrChild(srcNorm, dstNorm))
            {
                TerminalPrintLine32("cp: cannot copy a directory into itself", 0xFFFF0000U);
                return;
            }
            if (strlen(dstNorm) + strlen(slash) + 2U >= sizeof(finalDir))
            {
                TerminalPrintLine32("cp: destination path too long", 0xFFFF0000U);
                return;
            }
            strcpy(finalDir, dstNorm);
            CpStripTrailingSlash(finalDir);
            strcat(finalDir, "/");
            strcat(finalDir, slash);
        }
        else
        {
            strcpy(finalDir, dstNorm);
        }

        if (CpSameOrChild(srcNorm, finalDir))
        {
            TerminalPrintLine32("cp: cannot copy a directory into itself", 0xFFFF0000U);
            return;
        }

        strcpy(base, finalDir);
        if (!CpCopyTree(srcNorm, base))
            return;

        TerminalPrintLine32("cp: directory copied", 0x0057DB92U);
        return;
    }

    if (CpCopyFile(srcNorm, dstNorm))
        TerminalPrintLine32("cp: copied", 0x0057DB92U);
}

