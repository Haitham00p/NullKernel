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

#include "elf_loader.h"

#include <stddef.h>
#include "libc/string.h"
#include "mm/vmm.h"

#include <kernel/debug/debug.h>

#define ELF_MAGIC0        0x7FU
#define ELF_MAGIC1        'E'
#define ELF_MAGIC2        'L'
#define ELF_MAGIC3        'F'
#define ELFCLASS64        2U
#define ELFDATA2LSB       1U
#define EV_CURRENT        1U
#define ET_EXEC           2U
#define EM_X86_64         62U
#define PT_LOAD           1U
#define ELF64_EHDR_SIZE   64U
#define ELF64_PHDR_SIZE   56U

typedef struct
{
    uint8_t  Ident[16];
    uint16_t Type;
    uint16_t Machine;
    uint32_t Version;
    uint64_t Entry;
    uint64_t Phoff;
    uint64_t Shoff;
    uint32_t Flags;
    uint16_t Ehsize;
    uint16_t Phentsize;
    uint16_t Phnum;
    uint16_t Shentsize;
    uint16_t Shnum;
    uint16_t Shstrndx;
} Elf64Header;

typedef struct
{
    uint32_t Type;
    uint32_t Flags;
    uint64_t Offset;
    uint64_t Vaddr;
    uint64_t Paddr;
    uint64_t Filesz;
    uint64_t Memsz;
    uint64_t Align;
} Elf64Phdr;

static bool ElfIsPowerOfTwo(uint64_t Value)
{
    return Value >= 2 && (Value & (Value - 1ULL)) == 0;
}

bool ElfLoadImage(const void *Image, uint64_t ImageSize, ElfLoadInfo *Out)
{
    const Elf64Header *Header = (const Elf64Header *)Image;
    bool EntryExecutable = false;

    if (Image == NULL || Out == NULL || ImageSize < ELF64_EHDR_SIZE)
    {
        DbgSerialWrite("[elf] refuse: image too small\r\n");
        return false;
    }

    if (Header->Ident[0] != ELF_MAGIC0 || Header->Ident[1] != ELF_MAGIC1 ||
        Header->Ident[2] != ELF_MAGIC2 || Header->Ident[3] != ELF_MAGIC3 ||
        Header->Ident[4] != ELFCLASS64 || Header->Ident[5] != ELFDATA2LSB ||
        Header->Ident[6] != EV_CURRENT)
    {
        DbgSerialWrite("[elf] refuse: invalid ELF identification\r\n");
        return false;
    }

    if (Header->Type != ET_EXEC || Header->Machine != EM_X86_64 ||
        Header->Version != EV_CURRENT || Header->Ehsize != ELF64_EHDR_SIZE)
    {
        DbgSerialWrite("[elf] refuse: unsupported ELF type/machine/version\r\n");
        return false;
    }

    if (Header->Phentsize != ELF64_PHDR_SIZE || Header->Phnum == 0 ||
        Header->Phoff < ELF64_EHDR_SIZE || Header->Phoff > ImageSize ||
        Header->Phnum > (ImageSize - Header->Phoff) / ELF64_PHDR_SIZE)
    {
        DbgSerialWrite("[elf] refuse: program header table out of bounds\r\n");
        return false;
    }

    if (Header->Entry < ELF_PROGRAM_IMAGE_BASE ||
        Header->Entry >= ELF_PROGRAM_IMAGE_END)
    {
        DbgSerialWrite("[elf] refuse: entry outside reserved program slot\r\n");
        return false;
    }

    Out->Entry = Header->Entry;
    Out->LowestVaddr = UINT64_MAX;
    Out->HighestEnd = 0;

    /*
     * Validate every load segment before writing to memory. NLD ET_EXEC
     * images use a fixed 1-16 MiB identity-mapped slot, reserved from PMM.
     * They are trusted ring-0 guests, not isolated user processes.
     */
    for (uint64_t Index = 0; Index < Header->Phnum; Index++)
    {
        const Elf64Phdr *Phdr =
            (const Elf64Phdr *)((const uint8_t *)Image + Header->Phoff +
                                Index * ELF64_PHDR_SIZE);

        if (Phdr->Type != PT_LOAD)
            continue;

        if (Phdr->Memsz == 0 || Phdr->Filesz > Phdr->Memsz ||
            Phdr->Offset > ImageSize ||
            Phdr->Filesz > ImageSize - Phdr->Offset)
        {
            DbgSerialWrite("[elf] refuse: invalid PT_LOAD file/memory size\r\n");
            return false;
        }

        if (Phdr->Vaddr != Phdr->Paddr ||
            Phdr->Vaddr < ELF_PROGRAM_IMAGE_BASE ||
            Phdr->Memsz > UINT64_MAX - Phdr->Vaddr)
        {
            DbgSerialWrite("[elf] refuse: PT_LOAD outside fixed image slot\r\n");
            return false;
        }

        uint64_t SegmentEnd = Phdr->Vaddr + Phdr->Memsz;
        if (SegmentEnd > ELF_PROGRAM_IMAGE_END)
        {
            DbgSerialWrite("[elf] refuse: PT_LOAD exceeds reserved image slot\r\n");
            return false;
        }

        if (Phdr->Align > 1)
        {
            if (!ElfIsPowerOfTwo(Phdr->Align) ||
                (Phdr->Vaddr & (Phdr->Align - 1ULL)) !=
                (Phdr->Offset & (Phdr->Align - 1ULL)))
            {
                DbgSerialWrite("[elf] refuse: invalid PT_LOAD alignment\r\n");
                return false;
            }
        }

        /* The flat NLD image format requires sorted, non-overlapping loads. */
        if (Out->LowestVaddr != UINT64_MAX && Phdr->Vaddr < Out->HighestEnd)
        {
            DbgSerialWrite("[elf] refuse: overlapping/unsorted PT_LOAD segments\r\n");
            return false;
        }

        if ((Phdr->Flags & 1U) != 0 &&
            Header->Entry >= Phdr->Vaddr && Header->Entry < SegmentEnd)
            EntryExecutable = true;

        if (Phdr->Vaddr < Out->LowestVaddr)
            Out->LowestVaddr = Phdr->Vaddr;
        if (SegmentEnd > Out->HighestEnd)
            Out->HighestEnd = SegmentEnd;

        /* Verify the entire destination is identity mapped before writing. */
        uint64_t FirstPage = Phdr->Vaddr & ~0xFFFULL;
        uint64_t LastPage = (SegmentEnd - 1ULL) & ~0xFFFULL;
        for (uint64_t Page = FirstPage; Page <= LastPage; Page += 0x1000ULL)
        {
            if (VmmGetPhysical(Page) != Page)
            {
                DbgSerialWrite("[elf] refuse: program slot is not identity mapped\r\n");
                return false;
            }
        }
    }

    if (Out->LowestVaddr == UINT64_MAX || !EntryExecutable)
    {
        DbgSerialWrite("[elf] refuse: no load segment contains executable entry\r\n");
        return false;
    }

    /* Validation succeeded; now copy the image and zero each BSS tail. */
    for (uint64_t Index = 0; Index < Header->Phnum; Index++)
    {
        const Elf64Phdr *Phdr =
            (const Elf64Phdr *)((const uint8_t *)Image + Header->Phoff +
                                Index * ELF64_PHDR_SIZE);

        if (Phdr->Type != PT_LOAD)
            continue;

        memcpy((void *)(uintptr_t)Phdr->Vaddr,
               (const uint8_t *)Image + Phdr->Offset, Phdr->Filesz);
        if (Phdr->Memsz > Phdr->Filesz)
        {
            memset((void *)(uintptr_t)(Phdr->Vaddr + Phdr->Filesz), 0,
                   Phdr->Memsz - Phdr->Filesz);
        }
    }

    return true;
}
