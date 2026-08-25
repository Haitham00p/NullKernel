/* n64a — top-level assembler entry point */
#include "n64a.h"
#include "n64a_lex.h"
#include "n64a_parse.h"
#include "n64a_obj.h"

bool N64aAssemble(const char *FileName,
                  const char *Source, unsigned long long SourceSize,
                  uint8_t **OutObject, uint32_t *OutObjectSize,
                  N64aDiag *Diag)
{
    /* The assembler state embeds large symbol/fixup tables (~hundreds of
     * KiB); it must live on the heap, never on the (small) kernel stack. */
    N64aAsm *asmst = (N64aAsm *)N64A_MALLOC(sizeof(N64aAsm));
    N64Lex lx;
    uint32_t i;

    *OutObject = NULL;
    *OutObjectSize = 0;
    if (asmst == NULL)
    {
        N64aDiagInit(Diag, FileName);
        Diag->Failed = true;
        N64aStrCpy(Diag->Message, "out of memory");
        return false;
    }

    N64aDiagInit(Diag, FileName);
    N64aAsmInit(asmst, FileName, Diag);

    N64LexInit(&lx, FileName, Source, SourceSize, Diag);
    if (!N64aParse(asmst, &lx))
    {
        goto FAIL;
    }

    /* every fixup must reference a symbol that is either defined in this
     * object or declared extern; otherwise the name is a typo and the
     * assembler reports it immediately.                                  */
    for (i = 0; i < asmst->FixCount && !Diag->Failed; i++)
    {
        const char *name = asmst->Fixups[i].Name;
        if (N64aAsmFindSym(asmst, name) != NULL)
        {
            continue;
        }
        {
            bool is_ext = false;
            uint32_t j;
            for (j = 0; j < asmst->ExtCount; j++)
            {
                if (N64aStrCmp(asmst->Externs[j].Name, name) == 0)
                {
                    is_ext = true;
                    break;
                }
            }
            if (!is_ext)
            {
                Diag->Failed = true;
                Diag->Line = 0;
                N64aStrCpy(Diag->File, FileName);
                N64aStrCpy(Diag->Message, "undefined symbol");
                {
                    char *m = Diag->Message;
                    unsigned int k = 0;
                    while (m[k] != '\0' && k < sizeof(Diag->Message) - 4U)
                        k++;
                    m[k++] = ':';
                    m[k++] = ' ';
                    for (j = 0; name[j] != '\0' &&
                                k < sizeof(Diag->Message) - 1U; j++)
                    {
                        m[k++] = name[j];
                    }
                    m[k] = '\0';
                }
                goto FAIL;
            }
        }
    }
    if (Diag->Failed)
    {
        goto FAIL;
    }

    /* a 'global' declaration without a definition is an error */
    for (i = 0; i < asmst->SymCount && !Diag->Failed; i++)
    {
        if (asmst->Syms[i].Kind == 0U)
        {
            Diag->Failed = true;
            Diag->Line = 0;
            N64aStrCpy(Diag->File, FileName);
            N64aStrCpy(Diag->Message, "global symbol never defined");
            goto FAIL;
        }
    }

    /* main detection for the linker's entry-point choice */
    if (N64aAsmFindSym(asmst, "main") != NULL)
    {
        asmst->HasMain = true;
    }

    if (!N64aWriteObject(asmst, OutObject, OutObjectSize))
    {
        goto FAIL;
    }
    N64aAsmFree(asmst);
    N64A_FREE(asmst);
    return true;

FAIL:
    N64aAsmFree(asmst);
    N64A_FREE(asmst);
    return false;
}
