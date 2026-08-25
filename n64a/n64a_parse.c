/* n64a — statement parser and instruction dispatch.
 *
 * Grammar (line oriented, NASM-like subset):
 *
 *   line      := [label ':'] [stmt] [';' ...] NEWLINE
 *   label     := IDENT
 *   stmt      := directive | instruction
 *   directive := 'bits' 64 | 'global' names | 'extern' names
 *              | 'section' secname | db|dw|dd|dq expr-list
 *              | resb|resw|resd|resq count
 *   instr     := MNEMONIC [operand [, operand]]
 */
#include "n64a_parse.h"

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static void LowerCopy(char *dst, const char *src)
{
    uint32_t i = 0;
    for (; src[i] != '\0' && i < 31U; i++)
    {
        char c = src[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        dst[i] = c;
    }
    dst[i] = '\0';
}

static void ParseError(N64Lex *lx, const N64Tok *t,
                       const char *msg, const char *detail)
{
    N64aDiagAt(lx->Diag, t->Line, msg, detail);
}

static bool AtEol(N64Lex *lx)
{
    return lx->Diag->Failed || lx->Cur.Kind == N64_T_EOL ||
           lx->Cur.Kind == N64_T_EOF;
}

static void SkipEol(N64Lex *lx)
{
    while (!lx->Diag->Failed && lx->Cur.Kind == N64_T_EOL)
    {
        N64LexAdvance(lx);
    }
}

/* ------------------------------------------------------------------ */
/* expression evaluation                                               */
/* ------------------------------------------------------------------ */

typedef struct
{
    bool     HasSym;
    char     Sym[N64O_NAME_MAX];
    int64_t  Addend;    /* when symbolic: sym + addend                */
    uint64_t Value;     /* when not symbolic                          */
} N64Expr;

/* forward decl for parenthesised numeric subexpressions */
static bool ParseExpr(N64Lex *lx, N64Expr *e);

/* Parses one primary term with optional leading sign already applied by
 * the caller.  After the term, folds any trailing +/- NUMBER chains.   */
static bool ParseTermInto(N64Lex *lx, int64_t sign, N64Expr *e)
{
    e->HasSym = false;
    e->Sym[0] = '\0';
    e->Addend = 0;
    e->Value = 0;

    if (lx->Cur.Kind == N64_T_NUMBER || lx->Cur.Kind == N64_T_CHAR)
    {
        e->Value = lx->Cur.Num;
        N64LexAdvance(lx);
    }
    else if (lx->Cur.Kind == N64_T_IDENT)
    {
        N64Reg reg;
        if (N64RegFind(lx->Cur.Text, &reg))
        {
            ParseError(lx, &lx->Cur,
                       "register not allowed in this expression",
                       lx->Cur.Text);
            return false;
        }
        e->HasSym = true;
        N64aStrNCpy(e->Sym, lx->Cur.Text, sizeof(e->Sym));
        e->Addend = 0;
        N64LexAdvance(lx);
    }
    else if (lx->Cur.Kind == N64_T_LPAREN)
    {
        N64Expr inner;
        N64LexAdvance(lx);
        if (!ParseExpr(lx, &inner))
        {
            return false;
        }
        if (inner.HasSym)
        {
            ParseError(lx, &lx->Cur,
                       "symbolic expressions inside parentheses are "
                       "not supported", NULL);
            return false;
        }
        e->Value = inner.Value;
    }
    else
    {
        ParseError(lx, &lx->Cur, "expected expression", NULL);
        return false;
    }

    /* fold +/- constant chains:  sym+4, 1+2-3, etc. */
    while (lx->Cur.Kind == N64_T_PLUS || lx->Cur.Kind == N64_T_MINUS)
    {
        int dir = lx->Cur.Kind == N64_T_PLUS ? 1 : -1;
        N64LexAdvance(lx);
        if (lx->Cur.Kind != N64_T_NUMBER && lx->Cur.Kind != N64_T_CHAR)
        {
            ParseError(lx, &lx->Cur,
                       "only constant terms may follow a symbol", NULL);
            return false;
        }
        if (e->HasSym)
        {
            e->Addend += (int64_t)lx->Cur.Num * dir;
        }
        else
        {
            e->Value = (uint64_t)((int64_t)e->Value +
                                  (int64_t)lx->Cur.Num * dir);
        }
        N64LexAdvance(lx);
    }

    if (sign < 0)
    {
        if (e->HasSym)
        {
            ParseError(lx, &lx->Cur,
                       "negated symbols are not supported", NULL);
            return false;
        }
        e->Value = (uint64_t)(-(int64_t)e->Value);
    }
    return true;
}

static bool ParseExpr(N64Lex *lx, N64Expr *e)
{
    int64_t sign = 1;

    if (lx->Cur.Kind == N64_T_MINUS)
    {
        sign = -1;
        N64LexAdvance(lx);
    }
    else if (lx->Cur.Kind == N64_T_PLUS)
    {
        N64LexAdvance(lx);
    }
    return ParseTermInto(lx, sign, e);
}

/* ------------------------------------------------------------------ */
/* operand parsing                                                     */
/* ------------------------------------------------------------------ */

static void OperandInit(N64Operand *o)
{
    N64aMemSet(o, 0, sizeof(*o));
    o->Kind = N64_OP_NONE;
    o->SizeHint = 0;
}

static bool ParseMemOp(N64Lex *lx, N64Operand *o)
{
    N64MemOp *m = &o->Mem;
    int64_t sign = 1;

    m->Scale = 1;
    o->Kind = N64_OP_MEM;

    N64LexAdvance(lx); /* consume '[' */

    /* NASM-style 'rel'/'abs' prefix: consumed and ignored; [sym] is
     * always encoded as an absolute DISP32 with a relocation, which is
     * correct for the kernel's fixed-load user images. */
    if (lx->Cur.Kind == N64_T_IDENT &&
        (N64aStrCmp(lx->Cur.Text, "rel") == 0 ||
         N64aStrCmp(lx->Cur.Text, "abs") == 0))
    {
        N64LexAdvance(lx);
    }

    for (;;)
    {
        if (lx->Cur.Kind == N64_T_PLUS || lx->Cur.Kind == N64_T_MINUS)
        {
            sign = lx->Cur.Kind == N64_T_PLUS ? 1 : -1;
            N64LexAdvance(lx);
            continue;
        }

        if (lx->Cur.Kind == N64_T_IDENT)
        {
            N64Reg reg;
            if (N64RegFind(lx->Cur.Text, &reg))
            {
                if (reg.Size != N64_SZ64)
                {
                    ParseError(lx, &lx->Cur,
                               "memory addressing requires 64-bit "
                               "registers", lx->Cur.Text);
                    return false;
                }
                N64LexAdvance(lx);
                if (lx->Cur.Kind == N64_T_STAR)
                {
                    N64LexAdvance(lx);
                    if (lx->Cur.Kind != N64_T_NUMBER ||
                        (lx->Cur.Num != 2 && lx->Cur.Num != 4 &&
                         lx->Cur.Num != 8))
                    {
                        ParseError(lx, &lx->Cur,
                                   "scale must be 2, 4 or 8", NULL);
                        return false;
                    }
                    if (m->HasIndex)
                    {
                        ParseError(lx, &lx->Cur,
                                   "multiple index registers", NULL);
                        return false;
                    }
                    m->HasIndex = true;
                    m->IndexIdx = reg.Index;
                    m->Scale = (uint8_t)lx->Cur.Num;
                    N64LexAdvance(lx);
                }
                else if (!m->HasBase)
                {
                    m->HasBase = true;
                    m->BaseIdx = reg.Index;
                }
                else if (!m->HasIndex)
                {
                    m->HasIndex = true;
                    m->IndexIdx = reg.Index;
                    m->Scale = 1;
                }
                else
                {
                    ParseError(lx, &lx->Cur,
                               "too many registers in address", NULL);
                    return false;
                }
                continue;
            }

            /* symbol component of the address */
            if (m->Sym[0] != '\0')
            {
                ParseError(lx, &lx->Cur,
                           "multiple symbols in address", NULL);
                return false;
            }
            if (sign < 0)
            {
                ParseError(lx, &lx->Cur,
                           "cannot subtract symbols in address", NULL);
                return false;
            }
            N64aStrNCpy(m->Sym, lx->Cur.Text, sizeof(m->Sym));
            N64LexAdvance(lx);

            /* optional symbol+n / symbol-n */
            if (lx->Cur.Kind == N64_T_PLUS || lx->Cur.Kind == N64_T_MINUS)
            {
                int s2 = lx->Cur.Kind == N64_T_PLUS ? 1 : -1;
                N64LexAdvance(lx);
                if (lx->Cur.Kind != N64_T_NUMBER)
                {
                    ParseError(lx, &lx->Cur,
                               "expected constant after symbol offset",
                               NULL);
                    return false;
                }
                m->SymAddend = (int64_t)lx->Cur.Num * s2;
                N64LexAdvance(lx);
            }
            continue;
        }

        if (lx->Cur.Kind == N64_T_NUMBER || lx->Cur.Kind == N64_T_CHAR)
        {
            m->Disp += (int64_t)lx->Cur.Num * sign;
            N64LexAdvance(lx);
            sign = 1;
            continue;
        }

        break;
    }

    if (lx->Cur.Kind != N64_T_RBRACK)
    {
        ParseError(lx, &lx->Cur, "expected ']' in memory operand", NULL);
        return false;
    }
    N64LexAdvance(lx);
    return true;
}

static bool ParseOperand(N64Lex *lx, N64Operand *o)
{
    unsigned hint = 0;

    OperandInit(o);

    /* explicit size keywords before a memory operand */
    for (;;)
    {
        if (lx->Cur.Kind == N64_T_IDENT &&
            N64aStrCmp(lx->Cur.Text, "byte") == 0)
        { hint = N64_SZ8;  N64LexAdvance(lx); continue; }
        if (lx->Cur.Kind == N64_T_IDENT &&
            N64aStrCmp(lx->Cur.Text, "word") == 0)
        { hint = N64_SZ16; N64LexAdvance(lx); continue; }
        if (lx->Cur.Kind == N64_T_IDENT &&
            N64aStrCmp(lx->Cur.Text, "dword") == 0)
        { hint = N64_SZ32; N64LexAdvance(lx); continue; }
        if (lx->Cur.Kind == N64_T_IDENT &&
            N64aStrCmp(lx->Cur.Text, "qword") == 0)
        { hint = N64_SZ64; N64LexAdvance(lx); continue; }
        break;
    }

    if (lx->Cur.Kind == N64_T_LBRACK)
    {
        o->SizeHint = hint;
        return ParseMemOp(lx, o);
    }

    if (hint != 0U)
    {
        ParseError(lx, &lx->Cur,
                   "size keyword must precede a memory operand", NULL);
        return false;
    }

    if (lx->Cur.Kind == N64_T_IDENT)
    {
        N64Reg reg;
        if (N64RegFind(lx->Cur.Text, &reg))
        {
            o->Kind = N64_OP_REG;
            o->Reg = reg;
            N64LexAdvance(lx);
            return true;
        }
        o->Kind = N64_OP_IMM;
        N64aStrNCpy(o->Sym, lx->Cur.Text, sizeof(o->Sym));
        N64LexAdvance(lx);
        if (lx->Cur.Kind == N64_T_PLUS || lx->Cur.Kind == N64_T_MINUS)
        {
            int s = lx->Cur.Kind == N64_T_PLUS ? 1 : -1;
            N64LexAdvance(lx);
            if (lx->Cur.Kind != N64_T_NUMBER)
            {
                ParseError(lx, &lx->Cur,
                           "expected constant after symbol offset", NULL);
                return false;
            }
            o->SymAddend = (int64_t)lx->Cur.Num * s;
            N64LexAdvance(lx);
        }
        return true;
    }

    if (lx->Cur.Kind == N64_T_NUMBER || lx->Cur.Kind == N64_T_CHAR)
    {
        o->Kind = N64_OP_IMM;
        o->Imm = lx->Cur.Num;
        N64LexAdvance(lx);
        return true;
    }

    ParseError(lx, &lx->Cur, "expected operand", NULL);
    return false;
}

static bool ParseOperands(N64Lex *lx, N64Operand *ops, unsigned max,
                          unsigned *count)
{
    *count = 0;
    if (AtEol(lx))
    {
        return !lx->Diag->Failed;
    }
    for (;;)
    {
        if (*count >= max)
        {
            ParseError(lx, &lx->Cur, "too many operands", NULL);
            return false;
        }
        if (!ParseOperand(lx, &ops[*count]))
        {
            return false;
        }
        (*count)++;
        if (lx->Cur.Kind == N64_T_COMMA)
        {
            N64LexAdvance(lx);
            continue;
        }
        break;
    }
    if (!AtEol(lx))
    {
        ParseError(lx, &lx->Cur, "unexpected token after operands", NULL);
        return false;
    }
    return !lx->Diag->Failed;
}

/* ------------------------------------------------------------------ */
/* relocation sink (text section only; data fixups are added directly)  */
/* ------------------------------------------------------------------ */

static void SinkAddText(void *user, uint32_t offset, uint32_t type,
                        const char *name, int64_t addend)
{
    N64aAsmAddFixup((N64aAsm *)user, offset, type, name, addend,
                    N64_SEC_TEXT);
}

static void SinkInit(N64RelocSink *sink, N64aAsm *a)
{
    sink->User = a;
    sink->Add = SinkAddText;
}

/* ------------------------------------------------------------------ */
/* ALU opcode maps                                                     */
/* ext index == group digit for 80/81/83 and imm-accumulator forms    */
/* ------------------------------------------------------------------ */

typedef struct
{
    const char *Name;
    uint8_t     Ext;      /* ADD=0 OR=1 ADC=2 SBB=3 AND=4 SUB=5 XOR=6 CMP=7 */
} N64AluEntry;

static const N64AluEntry N64AluTable[] =
{
    { "add", 0 }, { "or", 1 }, { "adc", 2 }, { "sbb", 3 },
    { "and", 4 }, { "sub", 5 }, { "xor", 6 }, { "cmp", 7 }
};

#define N64_ALU_LEN (unsigned)(sizeof(N64AluTable) / sizeof(N64AluTable[0]))

static bool AluLookup(const char *name, uint8_t *ext)
{
    unsigned i;
    for (i = 0; i < N64_ALU_LEN; i++)
    {
        if (N64aStrCmp(N64AluTable[i].Name, name) == 0)
        {
            *ext = N64AluTable[i].Ext;
            return true;
        }
    }
    return false;
}

typedef struct
{
    const char *Name;
    uint8_t     Ext;   /* SHL=4 SHR=5 SAR=7 (SAL==SHL)                  */
} N64ShiftEntry;

static const N64ShiftEntry N64ShiftTable[] =
{
    { "shl", 4 }, { "sal", 4 }, { "shr", 5 }, { "sar", 7 }
};

#define N64_SHIFT_LEN \
    (unsigned)(sizeof(N64ShiftTable) / sizeof(N64ShiftTable[0]))

static bool ShiftLookup(const char *name, uint8_t *ext)
{
    unsigned i;
    for (i = 0; i < N64_SHIFT_LEN; i++)
    {
        if (N64aStrCmp(N64ShiftTable[i].Name, name) == 0)
        {
            *ext = N64ShiftTable[i].Ext;
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* instruction emitters (parser level)                                 */
/* ------------------------------------------------------------------ */

static bool EmitArith(N64aAsm *a, N64Lex *lx, uint8_t ext, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[2];
    unsigned n;
    uint8_t op_base = (uint8_t)(ext << 3); /* 00/08/10/... group base */

    if (!ParseOperands(lx, ops, 2, &n) || n != 2)
    {
        ParseError(lx, &lx->Cur, "arithmetic needs two operands", NULL);
        return false;
    }
    (void)line;

    if (ops[1].Kind == N64_OP_IMM && ops[1].Sym[0] == '\0')
    {
        bool acc = ops[0].Kind == N64_OP_REG &&
                   ops[0].Reg.Index == 0;
        unsigned size = ops[0].Kind == N64_OP_REG ?
                        ops[0].Reg.Size : ops[0].SizeHint;
        if (!N64EncArithImm(&a->Text, &sink, 0, ext, &ops[0],
                            ops[1].Imm, size ? size : N64_SZ32, acc))
        {
            N64aDiagAt(a->Diag, line,
                       "immediate does not fit operand size", NULL);
            return false;
        }
        return true;
    }

    /* symbolic immediate: movabs into rax then operate? Not supported
     * directly — clear diagnostic.                                       */
    if (ops[1].Kind == N64_OP_IMM)
    {
        N64aDiagAt(a->Diag, line,
                   "symbol not allowed here; load it with movabs first",
                   ops[1].Sym);
        return false;
    }

    if (!N64EncArithRegMem(&a->Text, &sink, 0, op_base, &ops[0], &ops[1]))
    {
        N64aDiagAt(a->Diag, line, "invalid operand combination", NULL);
        return false;
    }
    return true;
}

static bool EmitMov(N64aAsm *a, N64Lex *lx, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[2];
    unsigned n;

    if (!ParseOperands(lx, ops, 2, &n) || n != 2)
    {
        N64aDiagAt(a->Diag, line, "mov needs two operands", NULL);
        return false;
    }

    /* mov reg64, symbol  ->  movabs pattern (ABS64 reloc)              */
    if (ops[1].Kind == N64_OP_IMM && ops[1].Sym[0] != '\0')
    {
        if (ops[0].Kind != N64_OP_REG)
        {
            N64aDiagAt(a->Diag, line,
                       "symbolic source requires a register destination",
                       ops[1].Sym);
            return false;
        }
        if (ops[0].Reg.Size == N64_SZ64)
        {
            return N64EncMovAbs64(&a->Text, &sink, 0, &ops[0].Reg, 0,
                                  ops[1].Sym, ops[1].SymAddend);
        }
        if (ops[0].Reg.Size == N64_SZ32)
        {
            /* B8+r imm32 slot patched with ABS32 */
            N64EncRex(&a->Text,
                      ops[0].Reg.Index >= 8 ? N64_REX_B : 0U, false);
            N64BufByte(&a->Text,
                       (uint8_t)(0xB8U + (ops[0].Reg.Index & 7U)));
            N64aAsmAddFixup(a, a->Text.Size, N64O_R_ABS32,
                            ops[1].Sym, ops[1].SymAddend,
                            N64_SEC_TEXT);
            N64BufU32(&a->Text, 0);
            return true;
        }
        N64aDiagAt(a->Diag, line,
                   "symbolic values need a 32-bit or 64-bit register",
                   NULL);
        return false;
    }

    if (N64EncMov(&a->Text, &sink, 0, &ops[0], &ops[1]))
    {
        return true;
    }
    /* mov r64, big-imm fallback -> movabs */
    if (ops[0].Kind == N64_OP_REG && ops[0].Reg.Size == N64_SZ64 &&
        ops[1].Kind == N64_OP_IMM)
    {
        return N64EncMovAbs64(&a->Text, &sink, 0, &ops[0].Reg, ops[1].Imm,
                              NULL, 0);
    }
    N64aDiagAt(a->Diag, line, "invalid mov operands", NULL);
    return false;
}

static bool EmitMovabs(N64aAsm *a, N64Lex *lx, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[2];
    unsigned n;

    if (!ParseOperands(lx, ops, 2, &n) || n != 2 ||
        ops[0].Kind != N64_OP_REG || ops[0].Reg.Size != N64_SZ64)
    {
        N64aDiagAt(a->Diag, line,
                   "movabs requires r64 destination", NULL);
        return false;
    }
    if (ops[1].Kind != N64_OP_IMM)
    {
        N64aDiagAt(a->Diag, line, "movabs requires an immediate", NULL);
        return false;
    }
    return N64EncMovAbs64(&a->Text, &sink, 0, &ops[0].Reg, ops[1].Imm,
                          ops[1].Sym, ops[1].SymAddend);
}

static bool EmitTest(N64aAsm *a, N64Lex *lx, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[2];
    unsigned n;

    if (!ParseOperands(lx, ops, 2, &n) || n != 2)
    {
        N64aDiagAt(a->Diag, line, "test needs two operands", NULL);
        return false;
    }
    if (ops[1].Kind == N64_OP_IMM && ops[1].Sym[0] == '\0')
    {
        if (N64EncTestImm(&a->Text, &sink, 0, &ops[0], ops[1].Imm))
        {
            return true;
        }
    }
    else if (N64EncTest(&a->Text, &sink, 0, &ops[0], &ops[1]))
    {
        return true;
    }
    N64aDiagAt(a->Diag, line, "invalid test operands", NULL);
    return false;
}

static bool EmitLea(N64aAsm *a, N64Lex *lx, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[2];
    unsigned n;

    if (!ParseOperands(lx, ops, 2, &n) || n != 2 ||
        ops[0].Kind != N64_OP_REG ||
        (ops[0].Reg.Size != N64_SZ64 &&
         ops[0].Reg.Size != N64_SZ32) ||
        ops[1].Kind != N64_OP_MEM)
    {
        N64aDiagAt(a->Diag, line,
                   "lea requires 'lea r32/r64, [mem]'", NULL);
        return false;
    }
    return N64EncLea(&a->Text, &sink, 0, &ops[0].Reg, &ops[1].Mem);
}

static bool EmitUnaryOp(N64aAsm *a, const N64Operand *op, uint8_t ext,
                        uint32_t line)
{
    if (!N64EncUnaryGroup(&a->Text, ext, op))
    {
        N64aDiagAt(a->Diag, line, "unary instruction",
                   "requires a register operand");
        return false;
    }
    return true;
}

static bool EmitUnary(N64aAsm *a, N64Lex *lx, uint8_t ext, const char *mn,
                      uint32_t line)
{
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, ops, 1, &n) || n != 1)
    {
        N64aDiagAt(a->Diag, line, mn, "requires one operand");
        return false;
    }
    return EmitUnaryOp(a, &ops[0], ext, line);
}

static bool EmitIncDec(N64aAsm *a, N64Lex *lx, bool is_inc, uint32_t line)
{
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, ops, 1, &n) || n != 1 ||
        !N64EncIncDec(&a->Text, &ops[0], is_inc))
    {
        N64aDiagAt(a->Diag, line,
                   is_inc ? "inc" : "dec",
                   "requires a register operand");
        return false;
    }
    return true;
}

static bool EmitShift(N64aAsm *a, N64Lex *lx, uint8_t ext, uint32_t line)
{
    N64Operand ops[2];
    unsigned n;

    if (!ParseOperands(lx, ops, 2, &n) || n != 2 ||
        ops[0].Kind != N64_OP_REG)
    {
        N64aDiagAt(a->Diag, line, "shift requires register destination",
                   NULL);
        return false;
    }
    if (ops[1].Kind == N64_OP_REG && ops[1].Reg.Index == 1 &&
        ops[1].Reg.Size == N64_SZ8)
    {
        return N64EncShiftCl(&a->Text, ext, &ops[0]);
    }
    if (ops[1].Kind == N64_OP_IMM && ops[1].Sym[0] == '\0')
    {
        if (N64EncShiftImm(&a->Text, ext, &ops[0], ops[1].Imm))
        {
            return true;
        }
        N64aDiagAt(a->Diag, line, "shift count must be 0..63", NULL);
        return false;
    }
    N64aDiagAt(a->Diag, line,
               "second shift operand must be imm or cl", NULL);
    return false;
}

static bool EmitPush(N64aAsm *a, N64Lex *lx, uint32_t line)
{
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, ops, 1, &n) || n != 1)
    {
        N64aDiagAt(a->Diag, line, "push needs an operand", NULL);
        return false;
    }
    if (ops[0].Kind == N64_OP_REG)
    {
        if (N64EncPushReg(&a->Text, &ops[0].Reg))
        {
            return true;
        }
        N64aDiagAt(a->Diag, line, "push needs a 64-bit register", NULL);
        return false;
    }
    if (ops[0].Kind == N64_OP_IMM && ops[0].Sym[0] == '\0')
    {
        if (N64EncPushImm(&a->Text, ops[0].Imm))
        {
            return true;
        }
        N64aDiagAt(a->Diag, line, "push immediate too large", NULL);
        return false;
    }
    N64aDiagAt(a->Diag, line,
               "unsupported push operand (use movabs + push reg)", NULL);
    return false;
}

static bool EmitPop(N64aAsm *a, N64Lex *lx, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, ops, 1, &n) || n != 1)
    {
        N64aDiagAt(a->Diag, line, "pop needs an operand", NULL);
        return false;
    }
    if (ops[0].Kind == N64_OP_REG)
    {
        if (N64EncPopReg(&a->Text, &ops[0].Reg))
        {
            return true;
        }
        N64aDiagAt(a->Diag, line, "pop needs a 64-bit register", NULL);
        return false;
    }
    if (ops[0].Kind == N64_OP_MEM)
    {
        return N64EncPopMem(&a->Text, &sink, 0, &ops[0].Mem);
    }
    N64aDiagAt(a->Diag, line, "invalid pop operand", NULL);
    return false;
}

/* ------------------------------------------------------------------ */
/* control flow emitters                                               */
/* ------------------------------------------------------------------ */

/* Kernel symbols live at high addresses (0xFFFFFFFF8...), unreachable by
 * a rel32 call from the low user image.  For extern targets emit the
 * trampoline NCC uses: movabs rax,&sym ; mov r11,rax ; [xor eax,eax] ;
 * call r11 (or jmp r11). */
static bool AsmIsExtern(N64aAsm *a, const char *name)
{
    uint32_t i;
    for (i = 0; i < a->ExtCount; i++)
    {
        if (N64aStrCmp(a->Externs[i].Name, name) == 0)
        {
            return true;
        }
    }
    return false;
}

static bool EmitExternalBranch(N64aAsm *a, N64RelocSink *sink,
                               const char *sym, bool is_call,
                               uint32_t line)
{
    N64Reg rax, r11;
    N64Operand dst, src;

    if (!N64RegFind("rax", &rax) || !N64RegFind("r11", &r11))
    {
        N64aDiagAt(a->Diag, line, "internal: register lookup", NULL);
        return false;
    }

    /* movabs rax, sym */
    if (!N64EncMovAbs64(&a->Text, sink, 0, &rax, 0, sym, 0))
    {
        return false;
    }

    /* mov r11, rax */
    OperandInit(&dst);
    OperandInit(&src);
    dst.Kind = N64_OP_REG;
    dst.Reg = r11;
    src.Kind = N64_OP_REG;
    src.Reg = rax;
    if (!N64EncMov(&a->Text, sink, 0, &dst, &src))
    {
        N64aDiagAt(a->Diag, line, "internal: mov r11,rax", NULL);
        return false;
    }

    OperandInit(&dst);
    dst.Kind = N64_OP_REG;
    dst.Reg = r11;

    if (is_call)
    {
        /* xor eax, eax: varargs convention (matches NCC call sites) */
        N64Operand acc;
        OperandInit(&acc);
        acc.Kind = N64_OP_REG;
        if (!N64RegFind("eax", &acc.Reg) ||
            !N64EncArithImm(&a->Text, sink, 0, 6U, &acc, 0U, 4U, true))
        {
            N64aDiagAt(a->Diag, line, "internal: xor eax,eax", NULL);
            return false;
        }
        if (!N64EncCallRm(&a->Text, sink, 0, &dst))
        {
            N64aDiagAt(a->Diag, line, "internal: call r11", NULL);
            return false;
        }
    }
    else
    {
        if (!N64EncJmpRm(&a->Text, sink, 0, &dst))
        {
            N64aDiagAt(a->Diag, line, "internal: jmp r11", NULL);
            return false;
        }
    }
    return true;
}

static bool EmitCall(N64aAsm *a, N64Lex *lx, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, ops, 1, &n) || n != 1)
    {
        N64aDiagAt(a->Diag, line, "call needs an operand", NULL);
        return false;
    }
    if (ops[0].Kind == N64_OP_IMM)
    {
        if (ops[0].Sym[0] != '\0')
        {
            if (N64aAsmFindSym(a, ops[0].Sym) == NULL &&
                AsmIsExtern(a, ops[0].Sym))
            {
                /* extern: high kernel address, needs trampoline */
                return EmitExternalBranch(a, &sink, ops[0].Sym, true,
                                          line);
            }
            /* forward local target: rel32 relocation */
            return N64EncCallRel(&a->Text, &sink, 0, ops[0].Sym,
                                 ops[0].SymAddend, 0, false);
        }
        N64aDiagAt(a->Diag, line,
                   "absolute call targets are not supported; use labels",
                   NULL);
        return false;
    }
    if (N64EncCallRm(&a->Text, &sink, 0, &ops[0]))
    {
        return true;
    }
    N64aDiagAt(a->Diag, line, "invalid call operand", NULL);
    return false;
}

static bool EmitJmp(N64aAsm *a, N64Lex *lx, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, ops, 1, &n) || n != 1)
    {
        N64aDiagAt(a->Diag, line, "jmp needs an operand", NULL);
        return false;
    }
    if (ops[0].Kind == N64_OP_IMM)
    {
        if (ops[0].Sym[0] != '\0')
        {
            if (N64aAsmFindSym(a, ops[0].Sym) == NULL &&
                AsmIsExtern(a, ops[0].Sym))
            {
                return EmitExternalBranch(a, &sink, ops[0].Sym, false,
                                          line);
            }
            return N64EncJmpRel(&a->Text, &sink, 0, ops[0].Sym,
                                ops[0].SymAddend, 0, false);
        }
        N64aDiagAt(a->Diag, line,
                   "absolute jmp targets are not supported; use labels",
                   NULL);
        return false;
    }
    if (N64EncJmpRm(&a->Text, &sink, 0, &ops[0]))
    {
        return true;
    }
    N64aDiagAt(a->Diag, line, "invalid jmp operand", NULL);
    return false;
}

static bool EmitJcc(N64aAsm *a, N64Lex *lx, uint8_t cc, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, ops, 1, &n) || n != 1 ||
        ops[0].Kind != N64_OP_IMM)
    {
        N64aDiagAt(a->Diag, line,
                   "conditional jumps require a label target", NULL);
        return false;
    }
    return N64EncJcc(&a->Text, &sink, 0, cc, ops[0].Sym, ops[0].SymAddend,
                     ops[0].Imm, ops[0].Sym[0] == '\0');
}

static bool EmitSetcc(N64aAsm *a, N64Lex *lx, uint8_t cc, uint32_t line)
{
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, ops, 1, &n) || n != 1)
    {
        N64aDiagAt(a->Diag, line, "setcc needs an operand", NULL);
        return false;
    }
    if (N64EncSetcc(&a->Text, cc, &ops[0]))
    {
        return true;
    }
    N64aDiagAt(a->Diag, line,
               "setcc requires an 8-bit register destination", NULL);
    return false;
}

static bool EmitMovx(N64aAsm *a, N64Lex *lx, bool is_signed, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[2];
    unsigned n;

    if (!ParseOperands(lx, ops, 2, &n) || n != 2 ||
        ops[0].Kind != N64_OP_REG)
    {
        N64aDiagAt(a->Diag, line, is_signed ? "movsx" : "movzx",
                   "requires a register destination");
        return false;
    }
    if (N64EncMovx(&a->Text, &sink, 0, is_signed, &ops[0].Reg, &ops[1]))
    {
        return true;
    }
    N64aDiagAt(a->Diag, line,
               "invalid movzx/movsx operands (source must be r/m8 or "
               "r/m16)", NULL);
    return false;
}

/* ------------------------------------------------------------------ */
/* data directives                                                     */
/* ------------------------------------------------------------------ */

static bool EmitDataExpr(N64aAsm *a, N64Lex *lx, unsigned size,
                         uint32_t line)
{
    /* string literal (db only) */
    if (size == N64_SZ8 && lx->Cur.Kind == N64_T_STRING)
    {
        const char *p = lx->Cur.Str;
        uint32_t i;
        for (i = 0; i < lx->Cur.StrLen; i++)
        {
            char c = p[i];
            if (c == '\\' && i + 1U < lx->Cur.StrLen)
            {
                char e = p[++i];
                switch (e)
                {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '0': c = '\0'; break;
                    case '\\': c = '\\'; break;
                    case '"': c = '"'; break;
                    default: c = e; break;
                }
            }
            N64BufByte(&a->Data, (uint8_t)c);
        }
        N64LexAdvance(lx);
        return true;
    }

    {
        N64Expr e;
        if (!ParseExpr(lx, &e))
        {
            return false;
        }
        if (e.HasSym)
        {
            uint32_t type;
            switch (size)
            {
                case N64_SZ8:
                    N64aDiagAt(a->Diag, line,
                               "symbol values do not fit db", NULL);
                    return false;
                case N64_SZ16:
                    N64aDiagAt(a->Diag, line,
                               "symbol values do not fit dw", NULL);
                    return false;
                case N64_SZ32: type = N64O_R_ABS32; break;
                default:       type = N64O_R_ABS64; break;
            }
            N64aAsmAddFixup(a, a->Data.Size, type, e.Sym, e.Addend,
                            N64_SEC_DATA);
        }
        switch (size)
        {
            case N64_SZ8:  N64BufByte(&a->Data, (uint8_t)e.Value);  break;
            case N64_SZ16: N64BufU16(&a->Data, (uint16_t)e.Value);  break;
            case N64_SZ32: N64BufU32(&a->Data, (uint32_t)e.Value);  break;
            default:       N64BufU64(&a->Data, e.Value);            break;
        }
    }
    return true;
}

static bool EmitDataList(N64aAsm *a, N64Lex *lx, unsigned size,
                         const char *mn, uint32_t line)
{
    if (AtEol(lx))
    {
        N64aDiagAt(a->Diag, line, mn, "needs at least one value");
        return false;
    }
    for (;;)
    {
        if (!EmitDataExpr(a, lx, size, line))
        {
            return false;
        }
        if (lx->Cur.Kind == N64_T_COMMA)
        {
            N64LexAdvance(lx);
            continue;
        }
        break;
    }
    if (!AtEol(lx))
    {
        ParseError(lx, &lx->Cur, "unexpected token in data directive",
                   NULL);
        return false;
    }
    return !a->Diag->Failed;
}

static bool EmitRes(N64aAsm *a, N64Lex *lx, unsigned size,
                    uint32_t line)
{
    uint64_t count;
    uint64_t i;

    if (AtEol(lx) || lx->Cur.Kind != N64_T_NUMBER)
    {
        N64aDiagAt(a->Diag, line, "resX requires a count", NULL);
        return false;
    }
    count = lx->Cur.Num;
    N64LexAdvance(lx);
    for (i = 0; i < count; i++)
    {
        switch (size)
        {
            case N64_SZ8:  N64BufByte(&a->Data, 0);          break;
            case N64_SZ16: N64BufU16(&a->Data, 0);           break;
            case N64_SZ32: N64BufU32(&a->Data, 0);           break;
            default:       N64BufU64(&a->Data, 0);           break;
        }
    }
    return !a->Diag->Failed;
}

/* ------------------------------------------------------------------ */
/* directives                                                          */
/* ------------------------------------------------------------------ */

static bool DirNames(N64Lex *lx, N64aAsm *a, bool is_global, uint32_t line)
{
    for (;;)
    {
        if (lx->Cur.Kind != N64_T_IDENT)
        {
            N64aDiagAt(a->Diag, line,
                       is_global ? "global" : "extern",
                       "expects a symbol name");
            return false;
        }
        if (is_global)
        {
            if (!N64aAsmMarkGlobal(a, lx->Cur.Text))
            {
                N64aDiagAt(a->Diag, line, "too many symbols", NULL);
                return false;
            }
        }
        else
        {
            if (!N64aAsmAddExtern(a, lx->Cur.Text))
            {
                return false;
            }
        }
        N64LexAdvance(lx);
        if (lx->Cur.Kind == N64_T_COMMA)
        {
            N64LexAdvance(lx);
            continue;
        }
        break;
    }
    if (!AtEol(lx))
    {
        ParseError(lx, &lx->Cur, "unexpected token after symbol list",
                   NULL);
        return false;
    }
    return !a->Diag->Failed;
}

static bool DirSection(N64Lex *lx, N64aAsm *a, uint32_t line)
{
    if (lx->Cur.Kind != N64_T_IDENT)
    {
        N64aDiagAt(a->Diag, line, "section expects a name", NULL);
        return false;
    }
    if (N64aStrCmp(lx->Cur.Text, ".text") == 0 ||
        N64aStrCmp(lx->Cur.Text, ".code") == 0)
    {
        a->Section = N64_SEC_TEXT;
    }
    else if (N64aStrCmp(lx->Cur.Text, ".data") == 0 ||
             N64aStrCmp(lx->Cur.Text, ".rodata") == 0 ||
             N64aStrCmp(lx->Cur.Text, ".bss") == 0)
    {
        a->Section = N64_SEC_DATA;
    }
    else
    {
        N64aDiagAt(a->Diag, line,
                   "unsupported section (use .text or .data)",
                   lx->Cur.Text);
        return false;
    }
    N64LexAdvance(lx);
    if (!AtEol(lx))
    {
        ParseError(lx, &lx->Cur, "unexpected token after section name",
                   NULL);
        return false;
    }
    return !a->Diag->Failed;
}

/* ------------------------------------------------------------------ */
/* instruction dispatch                                                */
/* ------------------------------------------------------------------ */

static bool EmitInstruction(N64aAsm *a, N64Lex *lx, const char *raw_mn,
                            uint32_t line)
{
    char mn[40];
    uint8_t ext;
    uint8_t cc;
    N64RelocSink sink;

    LowerCopy(mn, raw_mn);
    SinkInit(&sink, a);

    /* ALU family */
    if (AluLookup(mn, &ext))
    {
        return EmitArith(a, lx, ext, line);
    }

    /* conditional jumps: j<cc> (jmp falls through to its own handler) */
    if (mn[0] == 'j' && mn[1] != '\0' && N64CondFind(&mn[1], &cc))
    {
        return EmitJcc(a, lx, cc, line);
    }

    /* set<cc> */
    if (mn[0] == 's' && mn[1] == 'e' && mn[2] == 't' &&
        N64CondFind(&mn[3], &cc))
    {
        return EmitSetcc(a, lx, cc, line);
    }

    /* shifts */
    if (ShiftLookup(mn, &ext))
    {
        return EmitShift(a, lx, ext, line);
    }

    if (N64aStrCmp(mn, "mov") == 0)    return EmitMov(a, lx, line);
    if (N64aStrCmp(mn, "movabs") == 0) return EmitMovabs(a, lx, line);
    if (N64aStrCmp(mn, "test") == 0)   return EmitTest(a, lx, line);
    if (N64aStrCmp(mn, "lea") == 0)    return EmitLea(a, lx, line);
    if (N64aStrCmp(mn, "push") == 0)   return EmitPush(a, lx, line);
    if (N64aStrCmp(mn, "pop") == 0)    return EmitPop(a, lx, line);
    if (N64aStrCmp(mn, "call") == 0)   return EmitCall(a, lx, line);
    if (N64aStrCmp(mn, "jmp") == 0)    return EmitJmp(a, lx, line);

    if (N64aStrCmp(mn, "inc") == 0)    return EmitIncDec(a, lx, true, line);
    if (N64aStrCmp(mn, "dec") == 0)    return EmitIncDec(a, lx, false, line);

    if (N64aStrCmp(mn, "not") == 0)    return EmitUnary(a, lx, 2, "not", line);
    if (N64aStrCmp(mn, "neg") == 0)    return EmitUnary(a, lx, 3, "neg", line);
    if (N64aStrCmp(mn, "mul") == 0)    return EmitUnary(a, lx, 4, "mul", line);
    if (N64aStrCmp(mn, "imul") == 0)
    {
        N64Operand ops[3];
        unsigned n;
        if (!ParseOperands(lx, ops, 3, &n))
        {
            return false;
        }
        if (n == 1)
        {
            return EmitUnaryOp(a, &ops[0], 5, line);   /* F7 /5 */
        }
        if (n == 2)
        {
            if (N64EncImulRR(&a->Text, &sink, 0, &ops[0].Reg, &ops[1]))
            {
                return true;
            }
            N64aDiagAt(a->Diag, line, "invalid imul operands", NULL);
            return false;
        }
        if (n == 3 && ops[1].Kind == N64_OP_REG &&
            ops[2].Kind == N64_OP_IMM && ops[2].Sym[0] == '\0')
        {
            if (N64EncImulRRI(&a->Text, &ops[0].Reg, &ops[1].Reg,
                              ops[2].Imm))
            {
                return true;
            }
            N64aDiagAt(a->Diag, line,
                       "immediate does not fit imul encoding", NULL);
            return false;
        }
        N64aDiagAt(a->Diag, line, "invalid imul operands", NULL);
        return false;
    }
    if (N64aStrCmp(mn, "div") == 0)    return EmitUnary(a, lx, 6, "div", line);
    if (N64aStrCmp(mn, "idiv") == 0)   return EmitUnary(a, lx, 7, "idiv", line);

    if (N64aStrCmp(mn, "movzx") == 0)  return EmitMovx(a, lx, false, line);
    if (N64aStrCmp(mn, "movsx") == 0)  return EmitMovx(a, lx, true, line);

    if (N64aStrCmp(mn, "ret") == 0)
    {
        N64Operand ops[1];
        unsigned n = 0;
        if (!AtEol(lx))
        {
            if (!ParseOperands(lx, ops, 1, &n))
            {
                return false;
            }
        }
        if (n == 0)
        {
            N64EncRet(&a->Text, 0, false);
            return true;
        }
        if (n == 1 && ops[0].Kind == N64_OP_IMM && ops[0].Sym[0] == '\0')
        {
            N64EncRet(&a->Text, ops[0].Imm, true);
            return true;
        }
        N64aDiagAt(a->Diag, line, "invalid ret operand", NULL);
        return false;
    }

    if (N64aStrCmp(mn, "leave") == 0)
    { N64EncLeave(&a->Text); return true; }
    if (N64aStrCmp(mn, "nop") == 0)
    { N64EncNop(&a->Text); return true; }
    if (N64aStrCmp(mn, "cdq") == 0)
    { N64EncCdqCqo(&a->Text, false); return true; }
    if (N64aStrCmp(mn, "cqo") == 0)
    { N64EncCdqCqo(&a->Text, true); return true; }
    if (N64aStrCmp(mn, "cwde") == 0)
    { N64EncCwdeCdqe(&a->Text, false); return true; }
    if (N64aStrCmp(mn, "cdqe") == 0)
    { N64EncCwdeCdqe(&a->Text, true); return true; }
    if (N64aStrCmp(mn, "hlt") == 0)
    { N64EncByteInst(&a->Text, 0xF4); return true; }
    if (N64aStrCmp(mn, "cli") == 0)
    { N64EncByteInst(&a->Text, 0xFA); return true; }
    if (N64aStrCmp(mn, "sti") == 0)
    { N64EncByteInst(&a->Text, 0xFB); return true; }
    if (N64aStrCmp(mn, "syscall") == 0)
    { N64EncByteInst(&a->Text, 0x0F); N64EncByteInst(&a->Text, 0x05);
      return true; }

    N64aDiagAt(a->Diag, line, "unknown instruction", raw_mn);
    return false;
}

/* ------------------------------------------------------------------ */
/* statement / line loop                                               */
/* ------------------------------------------------------------------ */

static bool ParseStatement(N64Lex *lx, N64aAsm *a)
{
    uint32_t line = lx->Cur.Line;

    if (lx->Cur.Kind == N64_T_IDENT)
    {
        /* label definition? */
        if (N64LexPeekIs(lx, N64_T_COLON))
        {
            uint32_t kind;
            uint32_t section = a->Section == N64_SEC_TEXT ?
                               N64_SEC_TEXT : N64_SEC_DATA;
            kind = a->Section == N64_SEC_TEXT ? N64O_SYM_FUNC :
                                                N64O_SYM_DATA;
            if (!N64aAsmDefineSym(a, lx->Cur.Text, kind, section,
                                  a->Section == N64_SEC_TEXT ?
                                  a->Text.Size : a->Data.Size))
            {
                return false;
            }
            N64LexAdvance(lx); /* ident */
            N64LexAdvance(lx); /* colon */
            if (AtEol(lx))
            {
                return !a->Diag->Failed;
            }
        }
    }

    if (AtEol(lx))
    {
        return !a->Diag->Failed;
    }

    if (lx->Cur.Kind == N64_T_IDENT)
    {
        char lower[40];
        LowerCopy(lower, lx->Cur.Text);

        if (N64aStrCmp(lower, "bits") == 0)
        {
            N64LexAdvance(lx);
            if (lx->Cur.Kind != N64_T_NUMBER || lx->Cur.Num != 64)
            {
                N64aDiagAt(a->Diag, line,
                           "only 'bits 64' is supported", NULL);
                return false;
            }
            N64LexAdvance(lx);
            if (!AtEol(lx))
            {
                ParseError(lx, &lx->Cur, "unexpected token after bits",
                           NULL);
                return false;
            }
            return !a->Diag->Failed;
        }
        if (N64aStrCmp(lower, "global") == 0)
        {
            N64LexAdvance(lx);
            return DirNames(lx, a, true, line);
        }
        if (N64aStrCmp(lower, "extern") == 0)
        {
            N64LexAdvance(lx);
            return DirNames(lx, a, false, line);
        }
        if (N64aStrCmp(lower, "section") == 0 ||
            N64aStrCmp(lower, "segment") == 0)
        {
            N64LexAdvance(lx);
            return DirSection(lx, a, line);
        }
        if (lower[0] == '.' )
        {
            /* bare ".text" style section marker */
            return DirSection(lx, a, line);
        }
        if (N64aStrCmp(lower, "db") == 0 || N64aStrCmp(lower, "byte") == 0)
        {
            N64LexAdvance(lx);
            return EmitDataList(a, lx, N64_SZ8, "db", line);
        }
        if (N64aStrCmp(lower, "dw") == 0 || N64aStrCmp(lower, "word") == 0)
        {
            N64LexAdvance(lx);
            return EmitDataList(a, lx, N64_SZ16, "dw", line);
        }
        if (N64aStrCmp(lower, "dd") == 0)
        {
            N64LexAdvance(lx);
            return EmitDataList(a, lx, N64_SZ32, "dd", line);
        }
        if (N64aStrCmp(lower, "dq") == 0)
        {
            N64LexAdvance(lx);
            return EmitDataList(a, lx, N64_SZ64, "dq", line);
        }
        if (N64aStrCmp(lower, "resb") == 0)
        {
            N64LexAdvance(lx);
            return EmitRes(a, lx, N64_SZ8, line);
        }
        if (N64aStrCmp(lower, "resw") == 0)
        {
            N64LexAdvance(lx);
            return EmitRes(a, lx, N64_SZ16, line);
        }
        if (N64aStrCmp(lower, "resd") == 0)
        {
            N64LexAdvance(lx);
            return EmitRes(a, lx, N64_SZ32, line);
        }
        if (N64aStrCmp(lower, "resq") == 0)
        {
            N64LexAdvance(lx);
            return EmitRes(a, lx, N64_SZ64, line);
        }
        if (N64aStrCmp(lower, "equ") == 0)
        {
            N64aDiagAt(a->Diag, line,
                       "'equ' is not supported yet", NULL);
            return false;
        }

        /* regular instruction */
        {
            char raw[64];
            N64aStrNCpy(raw, lx->Cur.Text, sizeof(raw));
            N64LexAdvance(lx);
            return EmitInstruction(a, lx, raw, line);
        }
    }

    ParseError(lx, &lx->Cur, "expected statement", NULL);
    return false;
}

bool N64aParse(N64aAsm *a, N64Lex *lx)
{
    N64LexAdvance(lx); /* prime Cur */
    while (!lx->Diag->Failed && lx->Cur.Kind != N64_T_EOF)
    {
        SkipEol(lx);
        if (lx->Diag->Failed || lx->Cur.Kind == N64_T_EOF)
        {
            break;
        }
        if (!ParseStatement(lx, a))
        {
            return false;
        }
    }
    return !lx->Diag->Failed;
}
