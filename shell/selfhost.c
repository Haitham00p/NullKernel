/* Copyright (C) 2026 Haitham Al-Nasser
 * This file is part of NullOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NullOS. If not, see <https://www.gnu.org/licenses/>.
 */

#include "libc/stdlib.h"
#include "libc/stdio.h"
#include "dispatcher.h"
#include "parser.h"

#include <stddef.h>
#include <stdarg.h>

#include "include/diskfs.h"
#include "lib/string/string.h"
#include "ncc/ncc.h"
#include "ncc/vault_gen.h"
#include "n64a/n64a.h"
#include "nld/nld.h"
#include "kernel/elf/elf_loader.h"
#include "arch/x86_64/program/run.h"
#include "mm/heap/heap.h"
#include "kernel/terminal/terminal.h"
#include "drivers/input/keyboard/keyboard.h"
#include "kernel/debug/debug.h"
#include "kernel/registry/registry.h"

extern uint32_t PromColor;
extern uint32_t BackSpaceColor;
extern uint32_t OutColor;
extern uint32_t PromptColor;

#define SELFHOST_MAX_IMAGE   (1U << 20)
#define SELFHOST_MAX_SOURCE  (1U << 15)
#define SELFHOST_MAX_LIST    (1U << 13)
#define SELFHOST_MAX_ARGS    16U
#define SELFHOST_MAX_ARG_LEN 4096U
#define SELFHOST_MAX_OBJECTS 16U

static uint32_t g_ArgCount;
static char g_Args[SELFHOST_MAX_ARGS][SELFHOST_MAX_ARG_LEN];

uint32_t ShellArgCount(void)
{
    return g_ArgCount;
}

bool ShellArgGet(uint32_t Index, char *Out, uint32_t MaxOut)
{
    if (Index >= g_ArgCount || Out == NULL || MaxOut == 0)
    {
        return false;
    }
    strncpy(Out, g_Args[Index], MaxOut - 1U);
    Out[MaxOut - 1U] = '\0';
    return true;
}

/* "HELLO.C" -> "HELLO.EXE", matching the FAT 8.3 short-name encoder
   (base truncated to 8 characters). */
void SelfHostExeName(const char *SourceName, char *ExeName)
{
    uint32_t Base = 0;

    while (SourceName[Base] != '\0' && SourceName[Base] != '.' &&
           Base < 8U)
    {
        Base++;
    }
    if (Base == 0)
    {
        Base = 1;
    }
    memcpy(ExeName, SourceName, Base);
    strcpy(ExeName + Base, ".EXE");
}

static bool SelfHostHasExtension(const char *Name, const char *Extension)
{
    uint32_t Length = strlen(Name);
    uint32_t ExtensionLength = strlen(Extension);

    return Length > ExtensionLength &&
           strcasecmp(Name + Length - ExtensionLength, Extension) == 0;
}

static void SelfHostPrintUInt32(uint32_t Value)
{
    char Buffer[12];
    uint32_t Length = 0;

    do
    {
        Buffer[Length++] = (char)('0' + (Value % 10U));
        Value /= 10U;
    } while (Value != 0 && Length < sizeof(Buffer));
    while (Length > 0)
    {
        TerminalPutChar32(Buffer[--Length], 0xFF00FF88);
    }
}

/* ------------------------------------------------------------------ */
/* mixed C + assembly builds (n64a objects linked through nld)          */
/* ------------------------------------------------------------------ */

static bool SelfHostVaultToNld(NldSymbol **OutVault, uint32_t *OutCount)
{
    NldSymbol *Vault = (NldSymbol *)kmalloc(
        (uint64_t)NccSymbolCount * sizeof(NldSymbol));
    uint32_t i;

    if (Vault == NULL)
    {
        return false;
    }
    for (i = 0; i < NccSymbolCount; i++)
    {
        Vault[i].Name = NccSymbols[i].Name;
        Vault[i].Addr = NccSymbols[i].Addr;
    }
    *OutVault = Vault;
    *OutCount = NccSymbolCount;
    return true;
}

/* Assembles an .ASM source from the working directory into a fresh
 * in-memory N64O object. */
bool SelfHostAssembleObject(const char *SourceName, uint8_t **OutObject,
                            uint32_t *OutSize)
{
    static char Source[SELFHOST_MAX_SOURCE];
    uint32_t Bytes = 0;
    N64aDiag Diag;

    *OutObject = NULL;
    *OutSize = 0;
    if (!DiskFsRead(SourceName, Source, sizeof(Source) - 1U, &Bytes))
    {
        TerminalWrite32(" read-fail", 0xFFFF5555);
        return false;
    }
    Source[Bytes] = '\0';
    if (!N64aAssemble(SourceName, Source, Bytes, OutObject, OutSize,
                      &Diag))
    {
        TerminalWrite32(" asm: ", 0xFFFF5555);
        TerminalPrintLine32(Diag.Message, 0xFFFF5555);
        return false;
    }
    return true;
}

/* Compiles a .C source from the working directory into a relocatable
 * object (same front end as a normal build; references stay symbolic). */
static bool SelfHostCompileOneObject(const char *SourceName,
                                     uint8_t **OutObject, uint32_t *OutSize)
{
    static char Source[SELFHOST_MAX_SOURCE];
    uint32_t Bytes = 0;

    *OutObject = NULL;
    *OutSize = 0;
    if (!DiskFsRead(SourceName, Source, sizeof(Source) - 1U, &Bytes))
    {
        TerminalWrite32(" read-fail", 0xFFFF5555);
        return false;
    }
    Source[Bytes] = '\0';
    if (!NccCompileObject(SourceName, Source, Bytes, OutObject, OutSize))
    {
        TerminalWrite32(" compile-fail", 0xFFFF5555);
        return false;
    }
    return true;
}

/* Links the given objects together with the kernel symbol vault and
 * writes the resulting ELF image as ExeName in the working directory. */
static bool SelfHostLinkAndStore(const char *ExeName,
                                 uint8_t **Objects, uint32_t *Sizes,
                                 const char **Names, uint32_t Count)
{
    NldSymbol *Vault = NULL;
    uint32_t VaultCount = 0;
    NldObject Inputs[SELFHOST_MAX_OBJECTS];
    uint8_t *ElfImage = NULL;
    uint32_t ElfSize = 0;
    NldDiag Diag;
    uint32_t i;
    bool Ok;

    if (!SelfHostVaultToNld(&Vault, &VaultCount))
    {
        TerminalPrintLine32(" link: out of memory", 0xFFFF5555);
        return false;
    }
    for (i = 0; i < Count && i < SELFHOST_MAX_OBJECTS; i++)
    {
        Inputs[i].Name = Names[i];
        Inputs[i].Data = Objects[i];
        Inputs[i].Size = Sizes[i];
    }
    Ok = NldLink(Inputs, Count, Vault, VaultCount,
                 &ElfImage, &ElfSize, &Diag);
    kfree(Vault);
    if (!Ok)
    {
        TerminalWrite32(" link: ", 0xFFFF5555);
        TerminalPrintLine32(Diag.Message, 0xFFFF5555);
        return false;
    }
    if (ElfSize > SELFHOST_MAX_IMAGE)
    {
        kfree(ElfImage);
        TerminalPrintLine32(" image too large", 0xFFFF5555);
        return false;
    }
    Ok = DiskFsWrite(ExeName, ElfImage, ElfSize);
    kfree(ElfImage);
    if (!Ok)
    {
        TerminalWrite32(" write-fail", 0xFFFF5555);
        return false;
    }
    return true;
}

/* Copies "NAME.C" -> "NAME" / "NAME.ASM" -> "NAME". */
static void SelfHostBaseName(const char *Name, char *Base)
{
    uint32_t i = 0;
    while (Name[i] != '\0' && Name[i] != '.' && i < 9U)
    {
        Base[i] = Name[i];
        i++;
    }
    Base[i] = '\0';
}

/* Shell/guest entry point: assemble SourceName (.ASM in the working
 * directory) into an N64O object stored as OutName. */
bool SelfHostAssembleFile(const char *SourceName, const char *OutName)
{
    uint8_t *Object = NULL;
    uint32_t Size = 0;
    bool Ok;

    if (!DiskFsIsMounted())
    {
        TerminalPrintLine32("n64a: no disk mounted", 0xFF5C5CFF);
        return false;
    }
    Ok = SelfHostAssembleObject(SourceName, &Object, &Size);
    if (!Ok)
    {
        return false;
    }
    Ok = DiskFsWrite(OutName, Object, Size);
    kfree(Object);
    if (!Ok)
    {
        TerminalPrintLine32("n64a: cannot write output", 0xFF5C5CFF);
        return false;
    }
    TerminalWrite32("n64a: wrote ", PromColor);
    TerminalPrintLine32(OutName, PromColor);
    return true;
}

/* Shell/guest entry point: link object files from the working directory
 * plus the kernel symbol vault into OutName (.EXE). */
bool SelfHostLinkFiles(const char **ObjectNames, uint32_t Count,
                       const char *OutName)
{
    static uint8_t *Objects[SELFHOST_MAX_OBJECTS];
    static uint32_t Sizes[SELFHOST_MAX_OBJECTS];
    bool Ok = true;

    if (!DiskFsIsMounted())
    {
        TerminalPrintLine32("nld: no disk mounted", 0xFF5C5CFF);
        return false;
    }
    if (Count == 0U || Count > SELFHOST_MAX_OBJECTS)
    {
        TerminalPrintLine32("nld: 1..16 objects supported", 0xFF5C5CFF);
        return false;
    }
    for (uint32_t i = 0; i < Count && Ok; i++)
    {
        uint32_t FileSize = 0;
        Objects[i] = NULL;
        Sizes[i] = 0;
        if (!DiskFsFileSize(ObjectNames[i], &FileSize) || FileSize == 0)
        {
            TerminalWrite32("nld: cannot read ", 0xFF5C5CFF);
            TerminalPrintLine32(ObjectNames[i], 0xFF5C5CFF);
            Ok = false;
            break;
        }
        Sizes[i] = FileSize;
        Objects[i] = (uint8_t *)kmalloc(Sizes[i]);
        if (Objects[i] == NULL)
        {
            Ok = false;
            break;
        }
        {
            uint32_t Got = 0;
            if (!DiskFsRead(ObjectNames[i], Objects[i], Sizes[i], &Got))
            {
                TerminalWrite32("nld: cannot read ", 0xFF5C5CFF);
                TerminalPrintLine32(ObjectNames[i], 0xFF5C5CFF);
                Ok = false;
            }
        }
    }
    if (Ok)
    {
        Ok = SelfHostLinkAndStore(OutName, Objects, Sizes,
                                  ObjectNames, Count);
        if (Ok)
        {
            TerminalWrite32("nld: wrote ", PromColor);
            TerminalPrintLine32(OutName, PromColor);
        }
    }
    for (uint32_t i = 0; i < Count; i++)
    {
        if (Objects[i] != NULL)
        {
            kfree(Objects[i]);
            Objects[i] = NULL;
        }
    }
    return Ok;
}

void ShellBootBuild(void)
{
    char *List = NULL;
    uint32_t Index = 0;
    uint32_t Built = 0;
    uint32_t Failed = 0;
    char SavedCwd[DISKFS_PATH_MAX];
    static char AsmBases[32][12];
    static uint32_t AsmBaseCount;

    TerminalPrintLine32("self-host build ...", 0xFF00FF88);
    DbgNcc("self-host build started", (uintptr_t)ShellBootBuild);

    if (!DiskFsMount())
    {
        TerminalPrintLine32("  no installed disk volume (run installer)",
                            0xFFFFAA00);
        return;
    }
    DiskFsPwd(SavedCwd, sizeof(SavedCwd));
    if (!DiskFsChdirPath("/BIN"))
    {
        TerminalPrintLine32("  /BIN missing on disk (run installer)",
                            0xFFFFAA00);
        return;
    }

    List = (char *)kmalloc(SELFHOST_MAX_LIST);
    if (List == NULL)
    {
        DiskFsChdirPath(SavedCwd);
        return;
    }
    DiskFsListNames(List, SELFHOST_MAX_LIST);

    /* collect assembly bases so sibling .ASM files pair with .C files */
    AsmBaseCount = 0;
    Index = 0;
    while (List[Index] != '\0')
    {
        uint32_t Start = Index;
        char Name[FAT32_SHORT_NAME_MAX + 1U];
        uint32_t Length;

        while (List[Index] != '\0' && List[Index] != '\n')
        {
            Index++;
        }
        Length = Index - Start;
        if (Length > 0 && Length < sizeof(Name))
        {
            memcpy(Name, &List[Start], Length);
            Name[Length] = '\0';
            if (SelfHostHasExtension(Name, ".ASM") &&
                AsmBaseCount < (sizeof(AsmBases) / sizeof(AsmBases[0])))
            {
                SelfHostBaseName(Name, AsmBases[AsmBaseCount]);
                AsmBaseCount++;
            }
        }
        if (List[Index] == '\n')
        {
            Index++;
        }
    }

    /* pass 1: every top-level .C builds an .EXE, pairing with a sibling
       .ASM of the same base name when one exists (mixed C + assembly) */
    Index = 0;
    while (List[Index] != '\0')
    {
        uint32_t Start = Index;
        char Name[FAT32_SHORT_NAME_MAX + 1U];
        uint32_t Length;

        while (List[Index] != '\0' && List[Index] != '\n')
        {
            Index++;
        }
        Length = Index - Start;
        if (Length > 0 && Length < sizeof(Name))
        {
            memcpy(Name, &List[Start], Length);
            Name[Length] = '\0';
            if (SelfHostHasExtension(Name, ".C"))
            {
                char Base[12];
                bool Mixed;
                uint8_t *Objects[SELFHOST_MAX_OBJECTS];
                uint32_t Sizes[SELFHOST_MAX_OBJECTS];
                const char *Names[SELFHOST_MAX_OBJECTS];
                uint32_t Count = 0;
                uint32_t a;
                uint32_t AsmIndex = UINT32_MAX;
                char ExeName[FAT32_SHORT_NAME_MAX + 1U];
                bool Ok;

                SelfHostBaseName(Name, Base);
                for (a = 0; a < AsmBaseCount; a++)
                {
                    if (strcmp(AsmBases[a], Base) == 0)
                    {
                        AsmIndex = a;
                        break;
                    }
                }
                Mixed = AsmIndex != UINT32_MAX;

                TerminalWrite32("  build ", 0xFFAAAAAA);
                TerminalWrite32(Name, 0xFFAAAAAA);
                TerminalWrite32(Mixed ? " (+asm) -> " : " -> ",
                                0xFFAAAAAA);
                SelfHostExeName(Name, ExeName);
                TerminalWrite32(ExeName, 0xFFAAAAAA);

                Ok = SelfHostCompileOneObject(Name, &Objects[0],
                                              &Sizes[0]);
                if (Ok)
                {
                    Names[Count++] = Name;
                }
                if (Ok && Mixed)
                {
                    char AsmName[FAT32_SHORT_NAME_MAX + 1U];
                    uint32_t bi = 0;
                    while (AsmBases[a][bi] != '\0')
                    {
                        AsmName[bi] = AsmBases[a][bi];
                        bi++;
                    }
                    strcpy(AsmName + bi, ".ASM");
                    TerminalWrite32("  asm   ", 0xFFAAAAAA);
                    TerminalWrite32(AsmName, 0xFFAAAAAA);
                    if (!SelfHostAssembleObject(AsmName, &Objects[Count],
                                                &Sizes[Count]))
                    {
                        Ok = false;
                    }
                    else
                    {
                        Names[Count++] = AsmName;
                    }
                }
                if (Ok &&
                    !SelfHostLinkAndStore(ExeName, Objects, Sizes,
                                          Names, Count))
                {
                    Ok = false;
                }
                for (a = 0; a < Count; a++)
                {
                    kfree(Objects[a]);
                }
                if (Ok)
                {
                    TerminalWrite32(" ok\n", 0xFF55FF55);
                    Built++;
                }
                else
                {
                    TerminalWrite32("\n", PromColor);
                    Failed++;
                }
            }
        }
        if (List[Index] == '\n')
        {
            Index++;
        }
    }

    /* pass 2: project directories — /BIN/<DIR> holding at least one .C
       file links every .C and .ASM inside it into /BIN/<DIR>.EXE */
    {
        char *DirList = (char *)kmalloc(SELFHOST_MAX_LIST);
        uint32_t di = 0;
        if (DirList != NULL)
        {
            DiskFsListNames(DirList, SELFHOST_MAX_LIST);
            while (DirList[di] != '\0')
            {
                uint32_t Start = di;
                char Name[FAT32_SHORT_NAME_MAX + 1U];
                uint32_t Length;

                while (DirList[di] != '\0' && DirList[di] != '\n')
                {
                    di++;
                }
                Length = di - Start;
                if (Length > 0 && Length < sizeof(Name))
                {
                    memcpy(Name, &DirList[Start], Length);
                    Name[Length] = '\0';
                    if (DiskFsIsDirectory(Name))
                    {
                        char ProjectExe[FAT32_SHORT_NAME_MAX + 1U];
                        char *Files = NULL;
                        uint32_t fi = 0;
                        uint8_t *Objects[SELFHOST_MAX_OBJECTS];
                        uint32_t Sizes[SELFHOST_MAX_OBJECTS];
                        const char *Names[SELFHOST_MAX_OBJECTS];
                        static char ObjPathNames[SELFHOST_MAX_OBJECTS]
                            [FAT32_SHORT_NAME_MAX + 1U];
                        uint32_t Count = 0;
                        bool Ok = true;

                        if (!DiskFsChdirPath(Name))
                        {
                            continue;
                        }
                        Files = (char *)kmalloc(SELFHOST_MAX_LIST);
                        if (Files == NULL)
                        {
                            DiskFsChdirPath("/BIN");
                            continue;
                        }
                        DiskFsListNames(Files, SELFHOST_MAX_LIST);
                        while (Files[fi] != '\0' && Ok)
                        {
                            uint32_t fStart = fi;
                            char FName[FAT32_SHORT_NAME_MAX + 1U];
                            uint32_t fLen;

                            while (Files[fi] != '\0' &&
                                   Files[fi] != '\n')
                            {
                                fi++;
                            }
                            fLen = fi - fStart;
                            if (fLen > 0 && fLen < sizeof(FName))
                            {
                                memcpy(FName, &Files[fStart], fLen);
                                FName[fLen] = '\0';
                                if (Count >= SELFHOST_MAX_OBJECTS)
                                {
                                    break;
                                }
                                if (SelfHostHasExtension(FName, ".C"))
                                {
                                    TerminalWrite32("  cc    ", 0xFFAAAAAA);
                                    TerminalWrite32(FName, 0xFFAAAAAA);
                                    TerminalWrite32("\n", PromColor);
                                    if (!SelfHostCompileOneObject(
                                            FName, &Objects[Count],
                                            &Sizes[Count]))
                                    {
                                        Ok = false;
                                    }
                                    else
                                    {
                                        strcpy(ObjPathNames[Count],
                                               FName);
                                        Names[Count] = ObjPathNames[Count];
                                        Count++;
                                    }
                                }
                                else if (SelfHostHasExtension(
                                             FName, ".ASM"))
                                {
                                    TerminalWrite32("  n64a  ", 0xFFAAAAAA);
                                    TerminalWrite32(FName, 0xFFAAAAAA);
                                    TerminalWrite32("\n", PromColor);
                                    if (!SelfHostAssembleObject(
                                            FName, &Objects[Count],
                                            &Sizes[Count]))
                                    {
                                        Ok = false;
                                    }
                                    else
                                    {
                                        strcpy(ObjPathNames[Count],
                                               FName);
                                        Names[Count] = ObjPathNames[Count];
                                        Count++;
                                    }
                                }
                            }
                            if (Files[fi] == '\n')
                            {
                                fi++;
                            }
                        }
                        kfree(Files);
                        if (Ok && Count != 0U)
                        {
                            SelfHostBaseName(Name, ProjectExe);
                            {
                                uint32_t pl = 0;
                                const char *Ext = ".EXE";
                                uint32_t e2 = 0;
                                while (ProjectExe[pl] != '\0') { pl++; }
                                while (Ext[e2] != '\0') { ProjectExe[pl++] = Ext[e2++]; }
                                ProjectExe[pl] = '\0';
                            }
                            TerminalWrite32("  link  ", 0xFFAAAAAA);
                            TerminalPrintLine32(ProjectExe, 0xFFAAAAAA);
                            if (SelfHostLinkAndStore(ProjectExe,
                                                     Objects, Sizes,
                                                     Names, Count))
                            {
                                Built++;
                            }
                            else
                            {
                                Failed++;
                            }
                        }
                        else if (!Ok)
                        {
                            Failed++;
                        }
                        for (uint32_t r = 0; r < Count; r++)
                        {
                            kfree(Objects[r]);
                        }
                        DiskFsChdirPath("/BIN");
                    }
                }
                if (DirList[di] == '\n')
                {
                    di++;
                }
            }
            kfree(DirList);
        }
    }

    kfree(List);
    DiskFsChdirPath(SavedCwd);

    TerminalWrite32("  built ", 0xFF00FF88);
    SelfHostPrintUInt32(Built);
    TerminalWrite32(" ok, ", 0xFF00FF88);
    SelfHostPrintUInt32(Failed);
    TerminalWrite32(" failed\n", 0xFF00FF88);
    {
        char Summary[64];
        uint32_t si = 0;
        const char *pre = "self-host build: ";
        const char *res = (Failed == 0) ? "all ok" : "errors present";
        while (pre[si] != '\0' && si < (sizeof(Summary) - 1U)) { Summary[si] = pre[si]; si++; }
        for (uint32_t k = 0; res[k] != '\0' && si < (sizeof(Summary) - 1U); k++) { Summary[si++] = res[k]; }
        Summary[si] = '\0';
        DbgNcc(Summary, 0U);
    }
}

/* Load and execute an executable stored at Dir/Name. */
static bool SelfHostRunExeInDir(const char *Dir, const char *Name)
{
    FAT32DirectoryEntry Entry;
    uint8_t *Image = NULL;
    uint32_t Bytes = 0;
    ElfLoadInfo Info;
    uint64_t Result;

    DbgSerialWrite("[EXEC] find ");
    DbgSerialWrite(Dir);
    DbgSerialWrite("/");
    DbgSerialWrite(Name);
    DbgSerialWrite("\n");

    if (!DiskFsFindInPath(Dir, Name, &Entry))
    {
        DbgSerialWrite("[EXEC] ERROR: file not found\n");
        return false;
    }
    if (Entry.FileSize == 0 || Entry.FileSize > SELFHOST_MAX_IMAGE)
    {
        DbgSerialWrite("[EXEC] ERROR: invalid image size\n");
        return false;
    }
    DbgSerialWrite("[EXEC] size ok, reading image\n");
    Image = (uint8_t *)kmalloc(Entry.FileSize);
    if (Image == NULL)
    {
        DbgSerialWrite("[EXEC] ERROR: kmalloc failed\n");
        return false;
    }
    if (!DiskFsReadInPath(Dir, Name, Image, Entry.FileSize, &Bytes))
    {
        DbgSerialWrite("[EXEC] ERROR: disk read failed\n");
        kfree(Image);
        return false;
    }
    DbgSerialWrite("[EXEC] image read, checking ELF magic\n");

    /* ROB contains both executable images and metadata (README.V, config
       files, etc.).  Never hand arbitrary registry data to the ELF loader. */
    if (Bytes < 4U ||
        Image[0] != 0x7FU ||
        Image[1] != 'E' ||
        Image[2] != 'L' ||
        Image[3] != 'F')
    {
        DbgSerialWrite("[EXEC] skip: not an ELF image\n");
        kfree(Image);
        return false;
    }

    DbgSerialWrite("[EXEC] ELF magic ok, loading ELF\n");
    if (!ElfLoadImage(Image, Bytes, &Info))
    {
        DbgSerialWrite("[EXEC] ERROR: ELF load failed\n");
        kfree(Image);
        return false;
    }
    DbgSerialWrite("[EXEC] ELF valid, entering program\n");
    g_ArgCount = 0;
    KbdAbortClear();
    Result = RunProgramOnStack(Info.Entry);
    DbgSerialWrite("[EXEC] program returned\n");
    kfree(Image);
    if (Result == NULLOS_GUEST_ABORTED)
    {
        TerminalPrintLine32("program aborted (Ctrl+C)", 0xFFFFAA00);
    }
    return true;
}

uint32_t readv(const char *Path, void *Buffer)
{
    uint32_t Size = 0;
    uint32_t Bytes = 0;
    const char *Slash = NULL;
    const char *Scan;

    if (Path == NULL || Buffer == NULL || !DiskFsIsMounted())
    {
        return 0;
    }
    for (Scan = Path; *Scan != '\0'; Scan++)
    {
        if (*Scan == '/')
        {
            Slash = Scan;
        }
    }
    if (Slash == NULL)
    {
        if (!DiskFsFileSize(Path, &Size) || Size == 0)
        {
            return 0;
        }
        if (!DiskFsRead(Path, Buffer, Size, &Bytes))
        {
            return 0;
        }
        return Bytes;
    }
    {
        char Dir[DISKFS_PATH_MAX];
        uint32_t DLen = (uint32_t)(Slash - Path);
        const char *Name = Slash + 1U;

        if (DLen >= sizeof(Dir) || *Name == '\0')
        {
            return 0;
        }
        memcpy(Dir, Path, DLen);
        Dir[DLen] = '\0';
        if (!DiskFsFileSizeInPath(Dir, Name, &Size) || Size == 0)
        {
            return 0;
        }
        if (!DiskFsReadInPath(Dir, Name, Buffer, Size, &Bytes))
        {
            return 0;
        }
        return Bytes;
    }
}

uint32_t writev(const char *Path, const char *Buffer)
{
    uint32_t Size;
    const char *Slash = NULL;
    const char *Scan;

    if (Path == NULL || Buffer == NULL || !DiskFsIsMounted())
    {
        return 0;
    }
    for (Scan = Path; *Scan != '\0'; Scan++)
    {
        if (*Scan == '/')
        {
            Slash = Scan;
        }
    }
    Size = (uint32_t)strlen(Buffer);
    if (Slash == NULL)
    {
        return DiskFsWrite(Path, Buffer, Size) ? 1U : 0U;
    }
    {
        char Dir[DISKFS_PATH_MAX];
        uint32_t DLen = (uint32_t)(Slash - Path);
        const char *Name = Slash + 1U;

        if (DLen >= sizeof(Dir) || *Name == '\0')
        {
            return 0;
        }
        memcpy(Dir, Path, DLen);
        Dir[DLen] = '\0';
        return DiskFsWriteInPath(Dir, Name, Buffer, Size) ? 1U : 0U;
    }
}

uint32_t createK(const char *Path)
{
    if (Path == NULL || !DiskFsIsMounted())
    {
        return 0;
    }
    return DiskFsTouch(Path) ? 1U : 0U;
}

uint32_t editv(const char *Path, const char *Buffer)
{
    uint32_t Size;
    uint32_t Existing = 0;

    if (Path == NULL || Buffer == NULL || !DiskFsIsMounted())
    {
        return 0;
    }
    if (!DiskFsFileSize(Path, &Existing))
    {
        return 0;
    }
    Size = (uint32_t)strlen(Buffer);
    return DiskFsWrite(Path, Buffer, Size) ? 1U : 0U;
}

uint64_t exec(const char *Path)
{
    FAT32DirectoryEntry Entry;
    uint8_t *Image = NULL;
    uint32_t Bytes = 0;
    ElfLoadInfo Info;
    uint64_t Result = 0;
    uint32_t Index = 0;
    uint32_t last = 0;
    const char *Name;
    const char *Dir;
    char Cwd[DISKFS_PATH_MAX];
    char ExeBuf[FAT32_SHORT_NAME_MAX + 1U];

    if (Path == NULL || !DiskFsIsMounted())
    {
        return 0;
    }
    if (Path[0] == '.' && Path[1] == '/')
    {
        DiskFsPwd(Cwd, sizeof(Cwd));
        SelfHostExeName(Path + 2U, ExeBuf);
        Dir = Cwd;
        Name = ExeBuf;
    }
    else
    {
        while (Path[Index] != '\0')
        {
            if (Path[Index] == '/')
            {
                last = Index;
            }
            Index++;
        }
        if (last == 0)
        {
            Dir = "/BIN";
            Name = Path;
        }
        else
        {
            Dir = Path;
            Name = Path + last + 1U;
        }
    }
    if (!DiskFsFindInPath(Dir, Name, &Entry))
    {
        /* No /BIN program matched.  For a bare command name, fall back to
           the built-in command dispatcher so guests can also launch kernel
           commands such as "regfill" or "ls" through exec(). */
        if (last == 0)
        {
            PARSED_COMMAND Cmd;
            ParserParse(Path, &Cmd);
            DispatcherDispatch(&Cmd);
            ParserFree(&Cmd);
        }
        return 0;
    }
    Image = (uint8_t *)kmalloc(SELFHOST_MAX_IMAGE);
    if (Image == NULL)
    {
        return 0;
    }
    if (!DiskFsReadInPath(Dir, Name, Image, SELFHOST_MAX_IMAGE, &Bytes))
    {
        kfree(Image);
        return 0;
    }
    if (!ElfLoadImage(Image, Bytes, &Info))
    {
        kfree(Image);
        return 0;
    }
    KbdAbortClear();
    Result = RunProgramOnStack(Info.Entry);
    kfree(Image);
    return Result;
}

/* Formatted variant of exec(): build the command line with printf-style
   formatting, then run it.  Example:
       execl("regfill %x %x", fgColor, bgColor); */
uint64_t execl(const char *fmt, ...)
{
    char Buf[256];
    va_list ap;
    int n;

    if (fmt == NULL)
    {
        return 0;
    }
    va_start(ap, fmt);
    n = vsnprintf(Buf, sizeof(Buf), fmt, ap);
    va_end(ap);
    if (n < 0)
    {
        return 0;
    }
    return exec(Buf);
}

/* Scans the on-disk registry (/REG) on every boot, like Windows, and applies
   any saved console theme. Creates the registry directory tree if missing so
   later writes (regfill) always succeed. */
void ShellApplyRegistry(void)
{
    uint8_t Buf[32];
    uint32_t N = 0;
    uint32_t Fg = 0xFFFFFFFFU;
    uint32_t Bg = 0x00000000U;
    char SavedCwd[64];

    if (!DiskFsIsMounted())
    {
        return;
    }
    DiskFsPwd(SavedCwd, sizeof(SavedCwd));
    DiskFsMkdirAll("/REG");
    DiskFsMkdirAll("/REG/color");
    DiskFsMkdirAll("/REG/NULLOS");
    DiskFsChdirPath(SavedCwd);
    DbgReg("registry scan started", (uintptr_t)ShellApplyRegistry);

    /* fg.v: foreground color (default 0xFFFFFFFF). */
    if (DiskFsReadInPath("/REG/color", "fg.v", Buf, sizeof(Buf) - 1U, &N))
    {
        Buf[N] = '\0';
        StringHexToUInt32((char *)Buf, &Fg);
        DbgReg((const char *)Buf, 0U);
    }
    else
    {
        DiskFsWriteInPath("/REG/color", "fg.v", "0xFFFFFFFF", 10);
        DbgReg("fg.v missing -> default 0xFFFFFFFF", 0U);
    }

    /* bg.v: background color (default 0x00000000). */
    if (DiskFsReadInPath("/REG/color", "bg.v", Buf, sizeof(Buf) - 1U, &N))
    {
        Buf[N] = '\0';
        StringHexToUInt32((char *)Buf, &Bg);
        DbgReg((const char *)Buf, 0U);
    }
    else
    {
        DiskFsWriteInPath("/REG/color", "bg.v", "0x00000000", 10);
        DbgReg("bg.v missing -> default 0x00000000", 0U);
    }

    PromColor = Fg;
    BackSpaceColor = Bg;
    DbgReg("applied console theme", 0U);
    TerminalClear32(Bg);

    /* op_fg.v: command output text color (printf/puts/cat). Default gray. */
    {
        uint32_t Out = 0x00808080U;
        if (DiskFsReadInPath("/REG/color", "op_fg.v", Buf, sizeof(Buf) - 1U, &N))
        {
            Buf[N] = '\0';
            StringHexToUInt32((char *)Buf, &Out);
            DbgReg((const char *)Buf, 0U);
        }
        else
        {
            DiskFsWriteInPath("/REG/color", "op_fg.v", "0x00808080", 10);
            DbgReg("op_fg.v missing -> default 0x00808080", 0U);
        }
        OutColor = Out;
    }

    /* p_fg.v: prompt color. Default white 0xFFFFFFFF (= none). */
    {
        uint32_t Prompt = 0xFFFFFFFFU;
        if (DiskFsReadInPath("/REG/color", "p_fg.v", Buf, sizeof(Buf) - 1U, &N))
        {
            Buf[N] = '\0';
            if (N > 0U)
            {
                StringHexToUInt32((char *)Buf, &Prompt);
            }
            DbgReg((const char *)Buf, 0U);
        }
        else
        {
            DiskFsWriteInPath("/REG/color", "p_fg.v", "0xFFFFFFFF", 10);
            DbgReg("p_fg.v missing -> default 0xFFFFFFFF", 0U);
        }
        PromptColor = Prompt;
    }
}

/* Reads /REG/DEFDIR.V (an absolute path such as "/HOME") and changes the
   shell's working directory to it once the OS has booted, so the user lands
   in their preferred directory instead of the filesystem root.  If the
   directory does not yet exist it is created first. */
void ShellBootRunROB(void)
{
    char *List = NULL;
    uint32_t Index = 0;
    uint32_t Ran = 0;
    char SavedCwd[DISKFS_PATH_MAX];
    char Enabled[8];

    if (!DiskFsIsMounted())
        return;

    /* ROB is controlled by the registry but its path is fixed and preserved. */
    if (RegistryReadText("/REG/BOOT/ROB.ENABLED.V", Enabled, sizeof(Enabled)) &&
        Enabled[0] == '0')
        return;

    List = (char *)kmalloc(SELFHOST_MAX_LIST);
    if (!List)
        return;

    DiskFsPwd(SavedCwd, sizeof(SavedCwd));
    if (!DiskFsChdirPath("/REG/NullOS/ROB"))
    {
        kfree(List);
        return;
    }

    DiskFsListNames(List, SELFHOST_MAX_LIST);
    DiskFsChdirPath(SavedCwd);

    while (List[Index] != '\0')
    {
        uint32_t Start = Index;
        char Name[FAT32_SHORT_NAME_MAX + 1U];
        uint32_t Length;

        while (List[Index] != '\0' && List[Index] != '\n')
            Index++;

        Length = Index - Start;
        if (Length > 0 && Length < sizeof(Name))
        {
            memcpy(Name, &List[Start], Length);
            Name[Length] = '\0';

            /* Directories are namespaces, not executable images. Every
               regular file is offered to the ELF loader; non-ELF files are
               simply ignored, so README/config files cannot break boot. */
            if (!DiskFsIsDirectory(Name))
            {
                FAT32DirectoryEntry Entry;
                if (DiskFsFindInPath("/REG/NullOS/ROB", Name, &Entry) &&
                    Entry.FileSize > 0 && Entry.FileSize <= SELFHOST_MAX_IMAGE)
                {
                    if (SelfHostRunExeInDir("/REG/NullOS/ROB", Name))
                        Ran++;
                }
            }
        }

        if (List[Index] == '\n')
            Index++;
    }

    DiskFsChdirPath(SavedCwd);
    kfree(List);

    TerminalWrite32("ROB: executable scan complete, ran ", PromColor);
    SelfHostPrintUInt32(Ran);
    TerminalPrintLine32(" image(s)", PromColor);
}

void ShellApplyDefDir(void)
{
    uint8_t Buf[DISKFS_PATH_MAX];
    uint32_t N = 0;
    char Dir[DISKFS_PATH_MAX];

    if (!DiskFsIsMounted())
    {
        return;
    }
    if (!DiskFsReadInPath("/REG", "DEFDIR.V", Buf, sizeof(Buf) - 1U, &N))
    {
        return;
    }
    if (N == 0)
    {
        return;
    }
    /* Trim trailing whitespace / newlines. */
    while (N > 0U &&
           (Buf[N - 1U] == '\n' || Buf[N - 1U] == '\r' ||
            Buf[N - 1U] == ' '  || Buf[N - 1U] == '\t'))
    {
        N--;
    }
    if (N == 0U || N >= sizeof(Dir))
    {
        return;
    }
    memcpy(Dir, Buf, N);
    Dir[N] = '\0';
    if (Dir[0] != '/')
    {
        return; /* only absolute paths are safe to chdir into */
    }
    if (!DiskFsChdirPath(Dir))
    {
        DiskFsMkdirAll(Dir);
        DiskFsChdirPath(Dir);
    }
}

bool DispatcherTryDiskCommand(const PARSED_COMMAND *Command)
{
    FAT32DirectoryEntry Entry;
    char ExeName[FAT32_SHORT_NAME_MAX + 1U];
    uint8_t *Image;
    uint32_t Bytes = 0;
    ElfLoadInfo Info;
    uint64_t Result;
    uint32_t Count;
    uint32_t Index;

    if (!DiskFsIsMounted() || Command == NULL ||
        Command->Command[0] == '\0')
    {
        return false;
    }

    const char *Dir;
    char Cwd[DISKFS_PATH_MAX];

    if (Command->Command[0] == '.' && Command->Command[1] == '/')
    {
        /* "./name": run <name>.EXE located in the current working directory,
           like a Unix "./program".  A bare command still resolves to /BIN. */
        uint32_t che = 0;
        bool IsExe = false;

        while (Command->Command[che] != '\0')
        {

        if (Command->Command[che] == '.')
        {
            if ((Command->Command[che + 1] == 'E' ||
                 Command->Command[che + 1] == 'e') &&
                (Command->Command[che + 2] == 'X' ||
                 Command->Command[che + 2] == 'x') &&
                (Command->Command[che + 3] == 'E' ||
                 Command->Command[che + 3] == 'e'))
            {
                IsExe = true;
                break;
            }
        }

        che++;
    }

    if (!IsExe)
    {
        TerminalPrintLine32("This isn't executable file, Aborting...\n", PromColor);
        return false;
    }

    DiskFsPwd(Cwd, sizeof(Cwd));
    SelfHostExeName(Command->Command + 2U, ExeName);
    Dir = Cwd;
    }
    else
    {
        SelfHostExeName(Command->Command, ExeName);
        Dir = "/BIN";
    }

    DbgSerialWrite("[EXEC] lookup ");
    DbgSerialWrite(Dir);
    DbgSerialWrite("/");
    DbgSerialWrite(ExeName);
    DbgSerialWrite("\n");
    if (!DiskFsFindInPath(Dir, ExeName, &Entry))
    {
        DbgSerialWrite("[EXEC] executable not found\n");
        return false;
    }
    if (Entry.FileSize == 0 || Entry.FileSize > SELFHOST_MAX_IMAGE)
    {
        DbgSerialWrite("[EXEC] invalid executable size\n");
        return false;
    }

    Image = (uint8_t *)kmalloc(Entry.FileSize);
    if (Image == NULL)
    {
        return false;
    }
    if (!DiskFsReadInPath(Dir, ExeName, Image, Entry.FileSize, &Bytes))
    {
        DbgSerialWrite("[EXEC] disk read failed\n");
        kfree(Image);
        return false;
    }
    if (!ElfLoadImage(Image, Bytes, &Info))
    {
        DbgSerialWrite("[EXEC] ELF validation failed\n");
        kfree(Image);
        return false;
    }
    DbgSerialWrite("[EXEC] ELF validated, entering program\n");

    Count = Command->ArgCount < SELFHOST_MAX_ARGS
                ? Command->ArgCount : SELFHOST_MAX_ARGS;
    g_ArgCount = Count;
    for (Index = 0; Index < Count; Index++)
    {
        strncpy(g_Args[Index], Command->Args[Index],
                SELFHOST_MAX_ARG_LEN - 1U);
        g_Args[Index][SELFHOST_MAX_ARG_LEN - 1U] = '\0';
    }

    KbdAbortClear();
    Result = RunProgramOnStack(Info.Entry);
    DbgSerialWrite("[EXEC] program returned\n");
    kfree(Image);

    if (Result == NULLOS_GUEST_ABORTED)
    {
        TerminalPrintLine32("program aborted (Ctrl+C)", 0xFFFFAA00);
    }
    return true;
}