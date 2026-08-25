#ifndef N64A_H
#define N64A_H

/* n64a — the NullOS x86-64 assembler.
 *
 * Independent assembler: own lexer, parser, register model, instruction
 * encoder and relocatable object writer.  No dependency on NASM/GNU as/
 * LLVM; it performs its own x86-64 encoding.
 *
 * Pipeline:
 *   source text -> lexer -> line parser -> per-instruction encoder
 *               -> fixups/relocations -> N64O relocatable object
 *
 * The N64O object is consumed by the NullOS linker (nld/link_core.c) which
 * combines n64a objects with NCC objects and the kernel symbol vault into
 * the final ELF .EXE image.
 */

#include "n64a_port.h"

/* ------------------------------------------------------------------ */
/* N64O relocatable object format                                      */
/* ------------------------------------------------------------------ */

#define N64O_MAGIC       "N64OBJ"
#define N64O_VERSION     1U
#define N64O_HDRSZ       48U
#define N64O_NAME_MAX    60U

/* Symbol kinds. */
#define N64O_SYM_FUNC    1U   /* global function, value = offset in text  */
#define N64O_SYM_DATA    2U   /* global data object, value = offset in data */

/* Section identifiers shared with the assembler internals. */
#define N64_SEC_TEXT     1U
#define N64_SEC_DATA     2U

/* Relocation types. All are resolved against the final virtual address of
 * the named symbol once the linker has assigned addresses. */
#define N64O_R_ABS64     1U   /* u64 at P <- S                (movabs imm) */
#define N64O_R_REL32     2U   /* s32 at P <- S - (P + 4)      (call/jmp)   */
#define N64O_R_DISP32    3U   /* u32 at P <- S, must fit 31:0 (mem [sym])  */
#define N64O_R_ABS32     4U   /* u32 at P <- S, must fit 31:0 (imm32 sym)  */

typedef struct
{
    char     Magic[8];        /* "N64OBJ\0"                            */
    uint16_t Version;
    uint16_t HeaderSize;
    uint32_t Flags;           /* bit0: defines "main"                  */
    uint32_t TextSize;
    uint32_t DataSize;
    uint32_t SymCount;
    uint32_t RelocCount;
    uint32_t Reserved0;
    uint32_t Reserved1;
} N64ObjHeader;

typedef struct
{
    uint64_t Value;           /* section offset                        */
    uint32_t Kind;            /* N64O_SYM_*                            */
    uint32_t Section;         /* 1 = text, 2 = data                    */
    char     Name[N64O_NAME_MAX];
} N64ObjSymbol;

typedef struct
{
    uint32_t Offset;          /* byte offset of the patch site in its section */
    uint32_t Type;            /* N64O_R_*                              */
    uint32_t Section;         /* N64_SEC_TEXT / N64_SEC_DATA           */
    uint32_t Reserved;
    int64_t  Addend;          /* added to the symbol address           */
    char     Name[N64O_NAME_MAX];
} N64ObjReloc;

/* ------------------------------------------------------------------ */
/* Diagnostics                                                         */
/* ------------------------------------------------------------------ */

typedef struct
{
    bool     Failed;
    char     File[96];
    uint32_t Line;
    char     Message[160];
} N64aDiag;

void N64aDiagInit(N64aDiag *d, const char *file);
void N64aDiagAt(N64aDiag *d, uint32_t line, const char *fmtpart,
                const char *detail);

/* ------------------------------------------------------------------ */
/* Assembler front end                                                 */
/* ------------------------------------------------------------------ */

/* Assembles Source and produces a N64O object blob allocated with the
 * port allocator.  Returns true on success.  On failure returns false and
 * fills Diag with file/line/message of the first error. */
bool N64aAssemble(const char *FileName,
                  const char *Source, unsigned long long SourceSize,
                  uint8_t **OutObject, uint32_t *OutObjectSize,
                  N64aDiag *Diag);

#endif /* N64A_H */
