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

/* nld — NullOS linker core (freestanding).
 *
 * Layout contract with ncc/ncc.c NccBuildElf():
 *   image[0x000..0x200)  ELF header + program header + padding
 *   image[0x200..)       data of all objects, then text of all objects,
 *                        object joins aligned to 16 bytes
 *   one PT_LOAD RWX at 0x100200, p_align 0x1000, entry = main
 *
 * Because the single segment is mapped so that VA == file offset +
 * (BASE), the file position of any virtual address is simply:
 *      file_pos(va) = va - NLD_IMAGE_BASE
 */
#include "nld.h"
#include "../n64a/n64a.h"

/* ------------------------------------------------------------------ */
/* little-endian helpers                                               */
/* ------------------------------------------------------------------ */

static uint16_t Rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t Rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void Wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static void Wr64(uint8_t *p, uint64_t v)
{
    unsigned i;
    for (i = 0; i < 8; i++)
    {
        p[i] = (uint8_t)((v >> (i * 8U)) & 0xFFU);
    }
}

/* ------------------------------------------------------------------ */
/* diagnostics                                                         */
/* ------------------------------------------------------------------ */

static bool Fail(NldDiag *d, const char *a, const char *b, const char *c)
{
    unsigned i = 0;
    if (d->Failed)
    {
        return false;
    }
    d->Failed = true;
    while (*a != '\0' && i < sizeof(d->Message) - 1U)
    {
        d->Message[i++] = *a++;
    }
    if (b != NULL)
    {
        while (*b != '\0' && i < sizeof(d->Message) - 1U)
        {
            d->Message[i++] = *b++;
        }
    }
    if (c != NULL)
    {
        if (i != 0U && i < sizeof(d->Message) - 1U)
        {
            d->Message[i++] = ' ';
        }
        while (*c != '\0' && i < sizeof(d->Message) - 1U)
        {
            d->Message[i++] = *c++;
        }
    }
    d->Message[i] = '\0';
    return false;
}

/* ------------------------------------------------------------------ */
/* parsed objects                                                      */
/* ------------------------------------------------------------------ */

typedef struct
{
    const char          *Name;
    const uint8_t       *Raw;
    uint32_t             RawSize;
    uint32_t             TextSize;
    uint32_t             DataSize;
    uint32_t             SymCount;
    uint32_t             RelocCount;
    bool                 HasMain;
    const uint8_t       *Text;
    const uint8_t       *Data;
    const N64ObjSymbol  *Syms;
    const N64ObjReloc   *Relocs;

    uint64_t TextVa;
    uint64_t DataVa;
} NldObj;

static bool ParseObj(NldObj *o, const NldObject *blob, NldDiag *d)
{
    o->Name = blob->Name != NULL ? blob->Name : "<object>";
    o->Raw = (const uint8_t *)blob->Data;
    o->RawSize = blob->Size;

    if (blob->Size < N64O_HDRSZ ||
        N64aStrCmp((const char *)o->Raw, N64O_MAGIC) != 0)
    {
        return Fail(d, "not a valid N64O object: ", o->Name, NULL);
    }
    if (Rd16(o->Raw + 8) != N64O_VERSION ||
        Rd16(o->Raw + 10) != N64O_HDRSZ)
    {
        return Fail(d, "unsupported object version: ", o->Name, NULL);
    }

    o->TextSize = Rd32(o->Raw + 16);
    o->DataSize = Rd32(o->Raw + 20);
    o->SymCount = Rd32(o->Raw + 24);
    o->RelocCount = Rd32(o->Raw + 28);
    o->HasMain = (Rd32(o->Raw + 12) & 1U) != 0U;

    {
        uint64_t need = (uint64_t)N64O_HDRSZ + o->TextSize + o->DataSize +
                        (uint64_t)o->SymCount * sizeof(N64ObjSymbol) +
                        (uint64_t)o->RelocCount * sizeof(N64ObjReloc);
        if (need > blob->Size)
        {
            return Fail(d, "truncated object: ", o->Name, NULL);
        }
    }

    o->Text = o->Raw + N64O_HDRSZ;
    o->Data = o->Text + o->TextSize;
    o->Syms = (const N64ObjSymbol *)(o->Data + o->DataSize);
    o->Relocs = (const N64ObjReloc *)((const uint8_t *)o->Syms +
                (uint64_t)o->SymCount * sizeof(N64ObjSymbol));
    return true;
}

/* ------------------------------------------------------------------ */
/* symbol table                                                        */
/* ------------------------------------------------------------------ */

typedef struct
{
    char     Name[N64O_NAME_MAX];
    uint64_t Addr;
} NldSymEnt;

#define NLD_MAX_SYMS 2048

static NldSymEnt g_Syms[NLD_MAX_SYMS];
static uint32_t g_SymCount;

static void SymReset(void)
{
    g_SymCount = 0;
}

static const NldSymEnt *SymFind(const char *name)
{
    uint32_t i;
    for (i = 0; i < g_SymCount; i++)
    {
        if (N64aStrCmp(g_Syms[i].Name, name) == 0)
        {
            return &g_Syms[i];
        }
    }
    return NULL;
}

static bool SymAdd(const char *name, uint64_t addr, NldDiag *d)
{
    if (g_SymCount >= NLD_MAX_SYMS)
    {
        return Fail(d, "too many symbols", NULL, NULL);
    }
    N64aStrNCpy(g_Syms[g_SymCount].Name, name,
                sizeof(g_Syms[0].Name));
    g_Syms[g_SymCount].Addr = addr;
    g_SymCount++;
    return true;
}

/* ------------------------------------------------------------------ */
/* linker                                                              */
/* ------------------------------------------------------------------ */

bool NldLink(const NldObject *Objects, uint32_t Count,
             const NldSymbol *Vault, uint32_t VaultCount,
             uint8_t **OutElf, uint32_t *OutElfSize,
             NldDiag *Diag)
{
    static NldObj objs[16];
    uint32_t i;
    uint32_t j;
    uint64_t cursor;
    uint64_t total_va;
    uint32_t total;
    uint64_t entry = 0;
    bool have_entry = false;
    uint8_t *img;

    Diag->Failed = false;
    Diag->Message[0] = '\0';
    *OutElf = NULL;
    *OutElfSize = 0;

    if (Objects == NULL || Count == 0 || Diag == NULL)
    {
        return Fail(Diag, "link: invalid arguments", NULL, NULL);
    }
    if (Count > (uint32_t)(sizeof(objs) / sizeof(objs[0])))
    {
        return Fail(Diag, "link: too many objects (max 16)", NULL, NULL);
    }

    /* ---- pass 1: parse --------------------------------------------- */
    for (i = 0; i < Count; i++)
    {
        if (!ParseObj(&objs[i], &Objects[i], Diag))
        {
            return false;
        }
    }

    /* ---- pass 2: layout (data then text, like NCC) ------------------ */
    cursor = NLD_IMAGE_BASE + NLD_IMAGE_HDRSZ;
    for (i = 0; i < Count; i++)
    {
        objs[i].DataVa = cursor;
        cursor += objs[i].DataSize;
        cursor = (cursor + NLD_ALIGN - 1ULL) &
                 ~(uint64_t)(NLD_ALIGN - 1ULL);
    }
    cursor = (cursor + NLD_ALIGN - 1ULL) & ~(uint64_t)(NLD_ALIGN - 1ULL);
    for (i = 0; i < Count; i++)
    {
        objs[i].TextVa = cursor;
        cursor += objs[i].TextSize;
        cursor = (cursor + NLD_ALIGN - 1ULL) &
                 ~(uint64_t)(NLD_ALIGN - 1ULL);
    }
    total_va = cursor - NLD_IMAGE_BASE;
    total = (uint32_t)total_va;

    /* ---- pass 3: symbol table -------------------------------------- */
    SymReset();
    for (i = 0; i < Count; i++)
    {
        for (j = 0; j < objs[i].SymCount; j++)
        {
            const N64ObjSymbol *s = &objs[i].Syms[j];
            if (SymFind(s->Name) != NULL)
            {
                return Fail(Diag, "duplicate symbol definition: ",
                            s->Name, NULL);
            }
            if (!SymAdd(s->Name,
                        (s->Section == N64_SEC_DATA ?
                         objs[i].DataVa : objs[i].TextVa) + s->Value,
                        Diag))
            {
                return false;
            }
        }
    }
    for (i = 0; i < VaultCount; i++)
    {
        if (Vault != NULL && Vault[i].Name != NULL &&
            SymFind(Vault[i].Name) == NULL)
        {
            if (!SymAdd(Vault[i].Name, Vault[i].Addr, Diag))
            {
                return false;
            }
        }
    }

    /* ---- pass 4: allocate image, copy sections --------------------- */
    img = (uint8_t *)N64A_MALLOC(total);
    if (img == NULL)
    {
        return Fail(Diag, "link: out of memory", NULL, NULL);
    }
    N64aMemSet(img, 0, total);

    for (i = 0; i < Count; i++)
    {
        if (objs[i].DataSize != 0U)
        {
            N64aMemCpy(&img[objs[i].DataVa - NLD_IMAGE_BASE],
                       objs[i].Data, objs[i].DataSize);
        }
        if (objs[i].TextSize != 0U)
        {
            N64aMemCpy(&img[objs[i].TextVa - NLD_IMAGE_BASE],
                       objs[i].Text, objs[i].TextSize);
        }
    }

    /* ---- entry point ------------------------------------------------ */
    /* prefer an object that declares main */
    for (i = 0; i < Count && !have_entry; i++)
    {
        if (objs[i].HasMain)
        {
            for (j = 0; j < objs[i].SymCount; j++)
            {
                if (N64aStrCmp(objs[i].Syms[j].Name, "main") == 0 &&
                    objs[i].Syms[j].Section == N64_SEC_TEXT)
                {
                    entry = objs[i].TextVa + objs[i].Syms[j].Value;
                    have_entry = true;
                    break;
                }
            }
        }
    }
    /* else a plain symbol named main anywhere */
    if (!have_entry)
    {
        const NldSymEnt *m = SymFind("main");
        if (m != NULL)
        {
            entry = m->Addr;
            have_entry = true;
        }
    }
    /* else the first text symbol of the first object (NCC fallback) */
    if (!have_entry)
    {
        for (i = 0; i < Count && !have_entry; i++)
        {
            for (j = 0; j < objs[i].SymCount; j++)
            {
                if (objs[i].Syms[j].Section == N64_SEC_TEXT)
                {
                    entry = objs[i].TextVa + objs[i].Syms[j].Value;
                    have_entry = true;
                    break;
                }
            }
        }
    }
    if (!have_entry)
    {
        N64A_FREE(img);
        return Fail(Diag, "no entry point: no 'main' found",
                    NULL, NULL);
    }

    /* ---- pass 5: apply relocations --------------------------------- */
    for (i = 0; i < Count; i++)
    {
        for (j = 0; j < objs[i].RelocCount; j++)
        {
            const N64ObjReloc *r = &objs[i].Relocs[j];
            const NldSymEnt *sym = SymFind(r->Name);
            uint64_t site_va;
            uint8_t *site;
            int64_t value;

            if (sym == NULL)
            {
                N64A_FREE(img);
                return Fail(Diag, "undefined symbol: ", r->Name,
                            objs[i].Name);
            }

            if (r->Section == N64_SEC_DATA)
            {
                site_va = objs[i].DataVa + r->Offset;
                if (r->Offset >= objs[i].DataSize ||
                    r->Type == N64O_R_REL32)
                {
                    N64A_FREE(img);
                    return Fail(Diag,
                                "invalid data relocation: ",
                                r->Name, objs[i].Name);
                }
            }
            else
            {
                site_va = objs[i].TextVa + r->Offset;
                if (r->Offset + 8U > objs[i].TextSize &&
                    r->Type != N64O_R_REL32)
                {
                    N64A_FREE(img);
                    return Fail(Diag,
                                "relocation out of range: ",
                                r->Name, objs[i].Name);
                }
                if (r->Offset + 4U > objs[i].TextSize &&
                    r->Type == N64O_R_REL32)
                {
                    N64A_FREE(img);
                    return Fail(Diag,
                                "relocation out of range: ",
                                r->Name, objs[i].Name);
                }
            }

            site = &img[site_va - NLD_IMAGE_BASE];

            switch (r->Type)
            {
                case N64O_R_ABS64:
                {
                    uint64_t v = sym->Addr + (uint64_t)r->Addend;
                    Wr64(site, v);
                    break;
                }
                case N64O_R_ABS32:
                {
                    uint64_t v = sym->Addr + (uint64_t)r->Addend;
                    if (v > 0x7FFFFFFFULL)
                    {
                        N64A_FREE(img);
                        return Fail(Diag,
                                    "address too far for 32-bit "
                                    "reference; use movabs: ",
                                    r->Name, objs[i].Name);
                    }
                    Wr32(site, (uint32_t)v);
                    break;
                }
                case N64O_R_DISP32:
                {
                    /* RIP-relative memory operand (lea rdi,[rel sym],
                       mov ...,[rel sym], etc.): disp32 is the signed delta
                       from the end of the instruction to the symbol. */
                    int64_t delta = (int64_t)sym->Addr +
                                    (int64_t)r->Addend -
                                    ((int64_t)site_va + 4);
                    if (delta < -2147483648LL || delta > 2147483647LL)
                    {
                        N64A_FREE(img);
                        return Fail(Diag,
                                    "displacement out of range: ",
                                    r->Name, objs[i].Name);
                    }
                    Wr32(site, (uint32_t)delta);
                    break;
                }
                case N64O_R_REL32:
                {
                    int64_t delta = (int64_t)sym->Addr +
                                    r->Addend -
                                    ((int64_t)site_va + 4);
                    if (delta < -2147483648LL || delta > 2147483647LL)
                    {
                        N64A_FREE(img);
                        return Fail(Diag,
                                    "call target out of range: ",
                                    r->Name, objs[i].Name);
                    }
                    Wr32(site, (uint32_t)delta);
                    break;
                }
                default:
                    N64A_FREE(img);
                    return Fail(Diag, "unknown relocation type: ",
                                r->Name, objs[i].Name);
            }
            value = 0;
            (void)value;
        }
    }

    /* ---- pass 6: ELF headers --------------------------------------- */
    Wr32(&img[0], 0x464C457FU); /* magic handled below byte-wise */
    img[0] = 0x7F; img[1] = 'E'; img[2] = 'L'; img[3] = 'F';
    img[4] = 2;                  /* ELFCLASS64    */
    img[5] = 1;                  /* ELFDATA2LSB   */
    img[6] = 1;                  /* EV_CURRENT    */
    img[7] = 0;                  /* ELFOSABI_NONE */
    Wr32(&img[16], 2);           /* e_type = ET_EXEC      */
    Wr32(&img[18], 62);          /* e_machine = EM_X86_64 */
    Wr32(&img[20], 1);           /* e_version     */
    Wr64(&img[24], entry);
    Wr64(&img[32], 64);          /* e_phoff       */
    Wr64(&img[40], 0);           /* e_shoff       */
    Wr32(&img[48], 0);           /* e_flags       */
    Wr32(&img[52], 64);          /* e_ehsize      */
    Wr32(&img[54], 56);          /* e_phentsize   */
    Wr32(&img[56], 1);           /* e_phnum       */
    Wr32(&img[58], 0);           /* e_shentsize   */
    Wr32(&img[60], 0);           /* e_shnum       */
    Wr32(&img[62], 0);           /* e_shstrndx    */

    {
        uint8_t *ph = &img[64];
        Wr32(&ph[0], 1);                                   /* PT_LOAD */
        Wr32(&ph[4], 7);                                   /* RWX     */
        Wr64(&ph[8], NLD_IMAGE_HDRSZ);                     /* offset  */
        Wr64(&ph[16], NLD_IMAGE_BASE + NLD_IMAGE_HDRSZ);   /* vaddr   */
        Wr64(&ph[24], NLD_IMAGE_BASE + NLD_IMAGE_HDRSZ);   /* paddr   */
        Wr64(&ph[32], total - NLD_IMAGE_HDRSZ);            /* filesz  */
        Wr64(&ph[40], total - NLD_IMAGE_HDRSZ);            /* memsz   */
        Wr64(&ph[48], 0x1000);                             /* align   */
    }

    *OutElf = img;
    *OutElfSize = total;
    return true;
}
