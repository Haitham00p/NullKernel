/* n64a — N64O relocatable object writer */
#include "n64a_obj.h"

static void Store16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static void Store32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

bool N64aWriteObject(N64aAsm *a, uint8_t **OutObject,
                     uint32_t *OutObjectSize)
{
    uint32_t i;
    uint32_t sym_bytes;
    uint32_t fix_bytes;
    uint32_t total;
    uint32_t off;
    uint8_t *img;

    /* only global symbols and referenced externs are exported; locals
     * that are not global stay in the symbol table as file-local info
     * but the object format keeps it simple: emit every defined symbol
     * (linker treats non-global ones as local to the object).          */

    /* count exported symbols: defined + marked global, plus externs
     * become undefined references through the fixups only.             */
    {
        uint32_t count = 0;
        for (i = 0; i < a->SymCount; i++)
        {
            if (a->Syms[i].Kind != 0U)
            {
                count++;
            }
        }
        sym_bytes = count * (uint32_t)sizeof(N64ObjSymbol);
    }
    fix_bytes = a->FixCount * (uint32_t)sizeof(N64ObjReloc);
    total = N64O_HDRSZ + a->Text.Size + a->Data.Size +
            sym_bytes + fix_bytes;

    img = (uint8_t *)N64A_MALLOC(total ? total : 1U);
    if (img == NULL)
    {
        N64aDiagAt(a->Diag, 0xFFFFFFFFU, "out of memory", NULL);
        return false;
    }
    N64aMemSet(img, 0, total);

    /* header */
    N64aMemCpy(img, N64O_MAGIC, 7);
    img[7] = '\0';
    Store16(&img[8], (uint16_t)N64O_VERSION);
    Store16(&img[10], (uint16_t)N64O_HDRSZ);
    Store32(&img[12], a->HasMain ? 1U : 0U);
    Store32(&img[16], a->Text.Size);
    Store32(&img[20], a->Data.Size);
    Store32(&img[24], sym_bytes / (uint32_t)sizeof(N64ObjSymbol));
    Store32(&img[28], a->FixCount);

    off = N64O_HDRSZ;
    if (a->Text.Size != 0U)
    {
        N64aMemCpy(&img[off], a->Text.Data, a->Text.Size);
        off += a->Text.Size;
    }
    if (a->Data.Size != 0U)
    {
        N64aMemCpy(&img[off], a->Data.Data, a->Data.Size);
        off += a->Data.Size;
    }

    /* symbols */
    for (i = 0; i < a->SymCount; i++)
    {
        N64ObjSymbol s;
        if (a->Syms[i].Kind == 0U)
        {
            continue; /* forward-declared but never defined */
        }
        N64aMemSet(&s, 0, sizeof(s));
        N64aStrNCpy(s.Name, a->Syms[i].Name, sizeof(s.Name));
        s.Value = a->Syms[i].Value;
        s.Kind = a->Syms[i].Kind;
        s.Section = a->Syms[i].Section;
        N64aMemCpy(&img[off], &s, sizeof(s));
        off += (uint32_t)sizeof(s);
    }

    /* relocations */
    for (i = 0; i < a->FixCount; i++)
    {
        N64ObjReloc r;
        N64aMemSet(&r, 0, sizeof(r));
        N64aStrNCpy(r.Name, a->Fixups[i].Name, sizeof(r.Name));
        r.Offset = a->Fixups[i].Offset;
        r.Type = a->Fixups[i].Type;
        r.Section = a->Fixups[i].Section;
        r.Addend = a->Fixups[i].Addend;
        N64aMemCpy(&img[off], &r, sizeof(r));
        off += (uint32_t)sizeof(r);
    }

    *OutObject = img;
    *OutObjectSize = total;
    return true;
}
