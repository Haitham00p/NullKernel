/* n64a — symbol / extern / relocation bookkeeping */
#include "n64a_asm.h"

void N64aAsmInit(N64aAsm *a, const char *file, N64aDiag *diag)
{
    N64aMemSet(a, 0, sizeof(*a));
    a->File = file;
    a->Diag = diag;
    a->Section = N64_SEC_TEXT;
    N64BufInit(&a->Text);
    N64BufInit(&a->Data);
}

void N64aAsmFree(N64aAsm *a)
{
    N64BufFree(&a->Text);
    N64BufFree(&a->Data);
}

N64Sym *N64aAsmFindSym(N64aAsm *a, const char *name)
{
    uint32_t i;
    for (i = 0; i < a->SymCount; i++)
    {
        if (N64aStrCmp(a->Syms[i].Name, name) == 0)
        {
            return &a->Syms[i];
        }
    }
    return NULL;
}

bool N64aAsmDefineSym(N64aAsm *a, const char *name, uint32_t kind,
                      uint32_t section, uint64_t value)
{
    N64Sym *s = N64aAsmFindSym(a, name);
    if (s != NULL && s->Kind != 0U && s->Value != value)
    {
        /* redefinition of an already defined label */
        N64aDiagAt(a->Diag, 0xFFFFFFFFU, "duplicate symbol", name);
        return false;
    }
    if (s == NULL)
    {
        if (a->SymCount >= N64_MAX_SYMS)
        {
            N64aDiagAt(a->Diag, 0xFFFFFFFFU, "too many symbols", name);
            return false;
        }
        s = &a->Syms[a->SymCount++];
        N64aStrNCpy(s->Name, name, sizeof(s->Name));
        s->IsGlobal = false;
    }
    /* Kind == 0 marks a forward 'global' declaration: fill it in now. */
    if (kind != 0U)
    {
        s->Kind = kind;
        s->Section = section;
    }
    s->Value = value;
    return true;
}

bool N64aAsmMarkGlobal(N64aAsm *a, const char *name)
{
    N64Sym *s = N64aAsmFindSym(a, name);
    if (s == NULL)
    {
        /* forward declaration of a global label defined later */
        if (a->SymCount >= N64_MAX_SYMS)
        {
            return false;
        }
        s = &a->Syms[a->SymCount++];
        N64aStrNCpy(s->Name, name, sizeof(s->Name));
        s->Kind = 0;
        s->Section = 0;
        s->IsGlobal = false;
        s->Value = 0;
    }
    s->IsGlobal = true;
    return true;
}

bool N64aAsmAddExtern(N64aAsm *a, const char *name)
{
    uint32_t i;
    for (i = 0; i < a->ExtCount; i++)
    {
        if (N64aStrCmp(a->Externs[i].Name, name) == 0)
        {
            return true;
        }
    }
    if (a->ExtCount >= N64_MAX_EXTERNS)
    {
        N64aDiagAt(a->Diag, 0xFFFFFFFFU, "too many extern symbols", name);
        return false;
    }
    N64aStrNCpy(a->Externs[a->ExtCount].Name, name,
                sizeof(a->Externs[0].Name));
    a->ExtCount++;
    return true;
}

N64Const *N64aAsmFindConst(N64aAsm *a, const char *name)
{
    uint32_t i;
    for (i = 0; i < a->ConstCount; i++)
    {
        if (N64aStrCmp(a->Consts[i].Name, name) == 0)
        {
            return &a->Consts[i];
        }
    }
    return NULL;
}

bool N64aAsmDefineConst(N64aAsm *a, const char *name, uint64_t value)
{
    N64Const *c = N64aAsmFindConst(a, name);
    if (c != NULL)
    {
        if (c->Value == value)
        {
            return true;
        }
        N64aDiagAt(a->Diag, 0xFFFFFFFFU, "duplicate constant", name);
        return false;
    }
    if (a->ConstCount >= N64_MAX_CONSTS)
    {
        N64aDiagAt(a->Diag, 0xFFFFFFFFU, "too many constants", name);
        return false;
    }
    c = &a->Consts[a->ConstCount++];
    N64aStrNCpy(c->Name, name, sizeof(c->Name));
    c->Value = value;
    return true;
}

void N64aAsmAddFixup(N64aAsm *a, uint32_t offset, uint32_t type,
                     const char *name, int64_t addend, uint32_t section)
{
    if (a->FixCount < N64_MAX_FIXUPS)
    {
        N64Fix *f = &a->Fixups[a->FixCount++];
        f->Offset = offset;
        f->Section = section;
        f->Type = type;
        N64aStrNCpy(f->Name, name, sizeof(f->Name));
        f->Addend = addend;
    }
    else
    {
        N64aDiagAt(a->Diag, 0xFFFFFFFFU, "too many fixups", NULL);
    }
}
