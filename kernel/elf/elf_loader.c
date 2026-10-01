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
#include <string.h>

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
#define ELF64_PHT_ALIGN   8U
#define KERNEL_SPACE_BASE 0xFFFF000000000000ULL
#define MAX_USER_END      0x000000FFFFFFFFFFFFULL

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
    const Elf64Header *Header;
    uint64_t i;

    if (Image == NULL || Out == NULL || ImageSize < ELF64_EHDR_SIZE)
    {
        DbgSerialWrite("[elf] refuse: image too small\r\n");
        return false;
    }

    Header = (const Elf64Header *)Image;
    if (Header->Ident[0] != ELF_MAGIC0 || Header->Ident[1] != ELF_MAGIC1 ||
        Header->Ident[2] != ELF_MAGIC2 || Header->Ident[3] != ELF_MAGIC3 ||
        Header->Ident[4] != ELFCLASS64 || Header->Ident[5] != ELFDATA2LSB)
    {
        DbgSerialWrite("[elf] refuse: not a little-endian x86-64 ELF\r\n");
        return false;
    }
    if (Header->Ident[6] != EV_CURRENT)
    {
        DbgSerialWrite("[elf] refuse: unsupported EI_VERSION\r\n");
        return false;
    }
    if (Header->Type != ET_EXEC)
    {
        DbgSerialWrite("[elf] refuse: not ET_EXEC\r\n");
        return false;
    }
    if (Header->Machine != EM_X86_64)
    {
        DbgSerialWrite("[elf] refuse: not EM_X86_64\r\n");
        return false;
    }
    if (Header->Version != EV_CURRENT)
    {
        DbgSerialWrite("[elf] refuse: wrong e_version\r\n");
        return false;
    }
    if (Header->Ehsize != ELF64_EHDR_SIZE)
    {
        DbgSerialWrite("[elf] refuse: bad e_ehsize\r\n");
        return false;
    }
    if (Header->Phentsize != ELF64_PHDR_SIZE || Header->Phnum == 0)
    {
        DbgSerialWrite("[elf] refuse: bad program header table\r\n");
        return false;
    }
    if (Header->Phoff < ELF64_EHDR_SIZE ||
        Header->Phoff + (uint64_t)Header->Phnum * ELF64_PHDR_SIZE > ImageSize)
    {
        DbgSerialWrite("[elf] refuse: program headers out of bounds\r\n");
        return false;
    }
    if (Header->Entry == 0 || Header->Entry >= KERNEL_SPACE_BASE)
    {
        DbgSerialWrite("[elf] refuse: entry point outside user space\r\n");
        return false;
    }

    Out->Entry = Header->Entry;
    Out->LowestVaddr = UINT64_MAX;
    Out->HighestEnd = 0;

    for (i = 0; i < Header->Phnum; i++)
    {
        const Elf64Phdr *Phdr =
            (const Elf64Phdr *)((const uint8_t *)Image + Header->Phoff +
                                i * ELF64_PHDR_SIZE);

        if (Phdr->Type != PT_LOAD)
        {
            continue;
        }
        if (!ElfIsPowerOfTwo(Phdr->Align))
        {
            DbgSerialWrite("[elf] refuse: segment alignment not power of two\r\n");
            return false;
        }
        if (Phdr->Vaddr != Phdr->Paddr)
        {
            DbgSerialWrite("[elf] refuse: flat loader needs Vaddr == Paddr\r\n");
            return false;
        }
        if (Phdr->Offset > ImageSize || Phdr->Filesz > ImageSize - Phdr->Offset)
        {
            DbgSerialWrite("[elf] refuse: segment file range out of bounds\r\n");
            return false;
        }
        if (Phdr->Filesz > Phdr->Memsz)
        {
            DbgSerialWrite("[elf] refuse: Filesz > Memsz\r\n");
            return false;
        }
        if (Phdr->Vaddr >= KERNEL_SPACE_BASE ||
            Phdr->Vaddr + Phdr->Memsz < Phdr->Vaddr)
        {
            DbgSerialWrite("[elf] refuse: segment wraps or enters kernel space\r\n");
            return false;
        }
        if (Phdr->Vaddr + Phdr->Memsz > MAX_USER_END)
        {
            DbgSerialWrite("[elf] refuse: segment above user maximum\r\n");
            return false;
        }
        if (Phdr->Align > 1 &&
            (Phdr->Vaddr & (Phdr->Align - 1ULL)) !=
            (Phdr->Offset & (Phdr->Align - 1ULL)))
        {
            DbgSerialWrite("[elf] refuse: vaddr/offset alignment mismatch\r\n");
            return false;
        }
        if (Out->LowestVaddr != UINT64_MAX && Phdr->Vaddr < Out->HighestEnd)
        {
            DbgSerialWrite("[elf] refuse: overlapping LOAD segments\r\n");
            return false;
        }
        if (Phdr->Vaddr + Phdr->Memsz <= Phdr->Vaddr)
        {
            DbgSerialWrite("[elf] refuse: segment size overflow\r\n");
            return false;
        }

        memcpy((void *)(uintptr_t)Phdr->Vaddr,
               (const uint8_t *)Image + Phdr->Offset, Phdr->Filesz);
        if (Phdr->Memsz > Phdr->Filesz)
        {
            memset((void *)(uintptr_t)(Phdr->Vaddr + Phdr->Filesz), 0,
                   Phdr->Memsz - Phdr->Filesz);
        }

        if (Phdr->Vaddr < Out->LowestVaddr)
        {
            Out->LowestVaddr = Phdr->Vaddr;
        }
        if (Phdr->Vaddr + Phdr->Memsz > Out->HighestEnd)
        {
            Out->HighestEnd = Phdr->Vaddr + Phdr->Memsz;
        }
    }

    return Out->LowestVaddr != UINT64_MAX;
}