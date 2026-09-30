#ifndef N64A_ASM_H
#define N64A_ASM_H

/* Assembler session state shared by the parser, the encoder (via the
 * relocation sink) and the object writer. */

#include "n64a_enc.h"

#define N64_MAX_SYMS    1024U
#define N64_MAX_FIXUPS  8192U
#define N64_MAX_EXTERNS 256U
#define N64_MAX_CONSTS  1024U

typedef struct
{
    char     Name[N64O_NAME_MAX];
    uint32_t Kind;        /* N64O_SYM_FUNC / N64O_SYM_DATA             */
    uint32_t Section;     /* N64_SEC_TEXT / N64_SEC_DATA               */
    uint64_t Value;       /* offset within the section                 */
    bool     IsGlobal;
} N64Sym;

typedef struct
{
    uint32_t Offset;      /* patch site within the section             */
    uint32_t Section;     /* N64_SEC_TEXT / N64_SEC_DATA               */
    uint32_t Type;        /* N64O_R_*                                  */
    char     Name[N64O_NAME_MAX];
    int64_t  Addend;
} N64Fix;

typedef struct
{
    char Name[N64O_NAME_MAX];
} N64Extern;

typedef struct
{
    char     Name[N64O_NAME_MAX];
    uint64_t Value;
} N64Const;

typedef struct
{
    const char *File;
    N64aDiag   *Diag;

    /* section buffers; relocations always refer to Text offsets */
    N64Buf      Text;
    N64Buf      Data;
    int         Section;   /* current emit section                      */

    N64Sym      Syms[N64_MAX_SYMS];
    uint32_t    SymCount;
    N64Fix      Fixups[N64_MAX_FIXUPS];
    uint32_t    FixCount;
    N64Extern   Externs[N64_MAX_EXTERNS];
    uint32_t    ExtCount;

    /* equ constants (value-only, not emitted as symbols) */
    N64Const    Consts[N64_MAX_CONSTS];
    uint32_t    ConstCount;

    bool        HasMain;
} N64aAsm;

void N64aAsmInit(N64aAsm *a, const char *file, N64aDiag *diag);
void N64aAsmFree(N64aAsm *a);

N64Sym *N64aAsmFindSym(N64aAsm *a, const char *name);
bool N64aAsmDefineSym(N64aAsm *a, const char *name, uint32_t kind,
                      uint32_t section, uint64_t value);
bool N64aAsmMarkGlobal(N64aAsm *a, const char *name);
bool N64aAsmAddExtern(N64aAsm *a, const char *name);
N64Const *N64aAsmFindConst(N64aAsm *a, const char *name);
bool N64aAsmDefineConst(N64aAsm *a, const char *name, uint64_t value);
void N64aAsmAddFixup(N64aAsm *a, uint32_t offset, uint32_t type,
                     const char *name, int64_t addend, uint32_t section);

#endif
