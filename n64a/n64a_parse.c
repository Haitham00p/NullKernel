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
static bool ParseExpr(N64Lex *lx, N64aAsm *a, N64Expr *e);

/* 'equ' constants, plus the special pseudo-symbols '$' (current offset)
 * and '$$' (section start, always 0 here).  Returns false when Name is
 * neither a constant nor one of the pseudo-symbols.                     */
static bool ConstLookup(N64aAsm *a, const char *name, uint64_t *out)
{
    N64Const *c;
    if (N64aStrCmp(name, "$") == 0)
    {
        *out = a->Section == N64_SEC_TEXT ? a->Text.Size : a->Data.Size;
        return true;
    }
    if (N64aStrCmp(name, "$$") == 0)
    {
        *out = 0;
        return true;
    }
    c = N64aAsmFindConst(a, name);
    if (c != NULL)
    {
        *out = c->Value;
        return true;
    }
    return false;
}

/* Constant-only subexpressions for 'equ'/'times'/'align'/'resX'.
 *
 *   expr    := bitor
 *   bitor   := bitxor { '|' bitxor }
 *   bitxor  := bitand { '^' bitand }
 *   bitand  := shift  { '&' shift }
 *   shift   := add    { ('<<' | '>>') add }
 *   add     := mul    { ('+' | '-') mul }
 *   mul     := unary  { ('*' | '/' | '%') unary }
 *   unary   := ('+' | '-' | '~' | '!') unary | primary
 *   primary := NUMBER | CHAR | IDENT(const/'$'/'$$') | '(' expr ')'
 *
 * Symbols are NOT allowed (i.e. no forward references to labels).  */
static bool ParseConstExpr(N64Lex *lx, N64aAsm *a, uint64_t *out);

static bool ParseConstPrimary(N64Lex *lx, N64aAsm *a, uint64_t *out)
{
    if (lx->Cur.Kind == N64_T_NUMBER || lx->Cur.Kind == N64_T_CHAR)
    {
        *out = lx->Cur.Num;
        N64LexAdvance(lx);
        return true;
    }
    if (lx->Cur.Kind == N64_T_IDENT)
    {
        if (ConstLookup(a, lx->Cur.Text, out))
        {
            N64LexAdvance(lx);
            return true;
        }
        ParseError(lx, &lx->Cur,
                   "expression is not constant (no forward references)",
                   lx->Cur.Text);
        return false;
    }
    if (lx->Cur.Kind == N64_T_LPAREN)
    {
        N64LexAdvance(lx);
        if (!ParseConstExpr(lx, a, out))
        {
            return false;
        }
        if (lx->Cur.Kind != N64_T_RPAREN)
        {
            ParseError(lx, &lx->Cur, "expected ')'", NULL);
            return false;
        }
        N64LexAdvance(lx);
        return true;
    }
    ParseError(lx, &lx->Cur, "expected constant term", NULL);
    return false;
}

static bool ParseConstUnary(N64Lex *lx, N64aAsm *a, uint64_t *out)
{
    if (lx->Cur.Kind == N64_T_MINUS)
    {
        N64LexAdvance(lx);
        if (!ParseConstUnary(lx, a, out))
        {
            return false;
        }
        *out = (uint64_t)(-(int64_t)*out);
        return true;
    }
    if (lx->Cur.Kind == N64_T_PLUS)
    {
        N64LexAdvance(lx);
        return ParseConstUnary(lx, a, out);
    }
    if (lx->Cur.Kind == N64_T_TILDE)
    {
        N64LexAdvance(lx);
        if (!ParseConstUnary(lx, a, out))
        {
            return false;
        }
        *out = ~(*out);
        return true;
    }
    if (lx->Cur.Kind == N64_T_BANG)
    {
        N64LexAdvance(lx);
        if (!ParseConstUnary(lx, a, out))
        {
            return false;
        }
        *out = (*out == 0U) ? 1U : 0U;
        return true;
    }
    return ParseConstPrimary(lx, a, out);
}

static bool ParseConstMul(N64Lex *lx, N64aAsm *a, uint64_t *out)
{
    uint64_t lhs;
    if (!ParseConstUnary(lx, a, &lhs))
    {
        return false;
    }
    for (;;)
    {
        if (lx->Cur.Kind == N64_T_STAR)
        {
            uint64_t rhs;
            N64LexAdvance(lx);
            if (!ParseConstUnary(lx, a, &rhs))
            {
                return false;
            }
            lhs *= rhs;
        }
        else if (lx->Cur.Kind == N64_T_SLASH)
        {
            uint64_t rhs;
            N64LexAdvance(lx);
            if (!ParseConstUnary(lx, a, &rhs))
            {
                return false;
            }
            if (rhs == 0U)
            {
                ParseError(lx, &lx->Cur, "division by zero", NULL);
                return false;
            }
            lhs /= rhs;
        }
        else if (lx->Cur.Kind == N64_T_PERCENT)
        {
            uint64_t rhs;
            N64LexAdvance(lx);
            if (!ParseConstUnary(lx, a, &rhs))
            {
                return false;
            }
            if (rhs == 0U)
            {
                ParseError(lx, &lx->Cur, "division by zero", NULL);
                return false;
            }
            lhs %= rhs;
        }
        else
        {
            break;
        }
    }
    *out = lhs;
    return true;
}

static bool ParseConstAdd(N64Lex *lx, N64aAsm *a, uint64_t *out)
{
    uint64_t lhs;
    if (!ParseConstMul(lx, a, &lhs))
    {
        return false;
    }
    for (;;)
    {
        if (lx->Cur.Kind == N64_T_PLUS)
        {
            uint64_t rhs;
            N64LexAdvance(lx);
            if (!ParseConstMul(lx, a, &rhs))
            {
                return false;
            }
            lhs += rhs;
        }
        else if (lx->Cur.Kind == N64_T_MINUS)
        {
            uint64_t rhs;
            N64LexAdvance(lx);
            if (!ParseConstMul(lx, a, &rhs))
            {
                return false;
            }
            lhs -= rhs;
        }
        else
        {
            break;
        }
    }
    *out = lhs;
    return true;
}

static bool ParseConstShift(N64Lex *lx, N64aAsm *a, uint64_t *out)
{
    uint64_t lhs;
    if (!ParseConstAdd(lx, a, &lhs))
    {
        return false;
    }
    for (;;)
    {
        if (lx->Cur.Kind == N64_T_LT && N64LexPeekIs(lx, N64_T_LT))
        {
            uint64_t rhs;
            N64LexAdvance(lx);
            N64LexAdvance(lx);
            if (!ParseConstAdd(lx, a, &rhs))
            {
                return false;
            }
            lhs <<= (rhs & 63U);
        }
        else if (lx->Cur.Kind == N64_T_GT && N64LexPeekIs(lx, N64_T_GT))
        {
            uint64_t rhs;
            N64LexAdvance(lx);
            N64LexAdvance(lx);
            if (!ParseConstAdd(lx, a, &rhs))
            {
                return false;
            }
            lhs >>= (rhs & 63U);
        }
        else
        {
            break;
        }
    }
    *out = lhs;
    return true;
}

static bool ParseConstBitAnd(N64Lex *lx, N64aAsm *a, uint64_t *out)
{
    uint64_t lhs;
    if (!ParseConstShift(lx, a, &lhs))
    {
        return false;
    }
    while (lx->Cur.Kind == N64_T_AMP)
    {
        uint64_t rhs;
        N64LexAdvance(lx);
        if (!ParseConstShift(lx, a, &rhs))
        {
            return false;
        }
        lhs &= rhs;
    }
    *out = lhs;
    return true;
}

static bool ParseConstBitXor(N64Lex *lx, N64aAsm *a, uint64_t *out)
{
    uint64_t lhs;
    if (!ParseConstBitAnd(lx, a, &lhs))
    {
        return false;
    }
    while (lx->Cur.Kind == N64_T_CARET)
    {
        uint64_t rhs;
        N64LexAdvance(lx);
        if (!ParseConstBitAnd(lx, a, &rhs))
        {
            return false;
        }
        lhs ^= rhs;
    }
    *out = lhs;
    return true;
}

static bool ParseConstBitOr(N64Lex *lx, N64aAsm *a, uint64_t *out)
{
    uint64_t lhs;
    if (!ParseConstBitXor(lx, a, &lhs))
    {
        return false;
    }
    while (lx->Cur.Kind == N64_T_PIPE)
    {
        uint64_t rhs;
        N64LexAdvance(lx);
        if (!ParseConstBitXor(lx, a, &rhs))
        {
            return false;
        }
        lhs |= rhs;
    }
    *out = lhs;
    return true;
}

static bool ParseConstExpr(N64Lex *lx, N64aAsm *a, uint64_t *out)
{
    return ParseConstBitOr(lx, a, out);
}

/* Parses one primary term with optional leading sign already applied by
 * the caller.  After the term, folds any trailing +/- NUMBER chains.   */
static bool ParseTermInto(N64Lex *lx, N64aAsm *a, int64_t sign, N64Expr *e)
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
        uint64_t k;
        N64Reg reg;
        if (ConstLookup(a, lx->Cur.Text, &k))
        {
            e->Value = k;
            N64LexAdvance(lx);
        }
        else if (N64RegFind(lx->Cur.Text, &reg))
        {
            ParseError(lx, &lx->Cur,
                       "register not allowed in this expression",
                       lx->Cur.Text);
            return false;
        }
        else
        {
            e->HasSym = true;
            N64aStrNCpy(e->Sym, lx->Cur.Text, sizeof(e->Sym));
            e->Addend = 0;
            N64LexAdvance(lx);
        }
    }
    else if (lx->Cur.Kind == N64_T_LPAREN)
    {
        N64Expr inner;
        N64LexAdvance(lx);
        if (!ParseExpr(lx, a, &inner))
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

static bool ParseExpr(N64Lex *lx, N64aAsm *a, N64Expr *e)
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
    return ParseTermInto(lx, a, sign, e);
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

static bool ParseMemOp(N64Lex *lx, N64aAsm *a, N64Operand *o)
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

            /* constant ('$'/'$$'/equ) component of the address */
            {
                uint64_t kv;
                if (ConstLookup(a, lx->Cur.Text, &kv))
                {
                    m->Disp += (int64_t)kv * sign;
                    N64LexAdvance(lx);
                    sign = 1;
                    continue;
                }
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

static bool ParseOperand(N64Lex *lx, N64aAsm *a, N64Operand *o)
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
        return ParseMemOp(lx, a, o);
    }

    if (hint != 0U)
    {
        ParseError(lx, &lx->Cur,
                   "size keyword must precede a memory operand", NULL);
        return false;
    }

    if (lx->Cur.Kind == N64_T_IDENT)
    {
        uint64_t kv;
        N64Reg reg;
        if (ConstLookup(a, lx->Cur.Text, &kv))
        {
            o->Kind = N64_OP_IMM;
            o->Imm = kv;
            N64LexAdvance(lx);
            return true;
        }
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

static bool ParseOperands(N64Lex *lx, N64aAsm *a, N64Operand *ops,
                          unsigned max, unsigned *count)
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
        if (!ParseOperand(lx, a, &ops[*count]))
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
    uint8_t     Ext;   /* SHL=4 SHR=5 SAR=7 (SAL==SHL), rotates ROL=0 ROR=1 RCL=2 RCR=3 */
} N64ShiftEntry;

static const N64ShiftEntry N64ShiftTable[] =
{
    { "rol", 0 }, { "ror", 1 }, { "rcl", 2 }, { "rcr", 3 },
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

/* true for mnemonics that begin with a conditional-cc suffix            */
static bool IsCondccMnemonic(const char *mn)
{
    return (mn[0] == 'j' && mn[1] != '\0' && N64CondFind(&mn[1], NULL)) ||
           (mn[0] == 's' && mn[1] == 'e' && mn[2] == 't' &&
            mn[3] != '\0' && N64CondFind(&mn[3], NULL)) ||
           (mn[0] == 'c' && mn[1] == 'm' && mn[2] == 'o' &&
            mn[3] == 'v' && N64CondFind(&mn[4], NULL));
}

/* whole instruction-mnemonic set (used to disambiguate colon-less
   labels such as 'mov db 5' vs 'mov byte [mem], imm')                  */
static bool IsInstructionMnemonic(const char *name)
{
    static const char *const Mn[] = {
        "add", "adc", "and", "bsf", "bsr", "bt", "btc", "btr", "bts",
        "call", "cdq", "cdqe", "cli", "cmp", "cqo", "cwde", "dec", "div",
        "hlt", "idiv", "imul", "inc", "jmp", "lea", "leave", "mov",
        "movabs", "movsx", "movzx", "mul", "neg", "nop", "not", "or",
        "pop", "push", "ret", "rol", "ror", "rcl", "rcr", "sal", "sar",
        "sbb", "shl", "shr", "sti", "sub", "syscall", "test", "xchg",
        "xor"
    };
    unsigned i;
    if (IsCondccMnemonic(name))
    {
        return true;
    }
    for (i = 0; i < (unsigned)(sizeof(Mn) / sizeof(Mn[0])); i++)
    {
        if (N64aStrCmp(Mn[i], name) == 0)
        {
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

    if (!ParseOperands(lx, a, ops, 2, &n) || n != 2)
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

    if (!ParseOperands(lx, a, ops, 2, &n) || n != 2)
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

    if (!ParseOperands(lx, a, ops, 2, &n) || n != 2 ||
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

    if (!ParseOperands(lx, a, ops, 2, &n) || n != 2)
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

    if (!ParseOperands(lx, a, ops, 2, &n) || n != 2 ||
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
    N64RelocSink sink;
    SinkInit(&sink, a);
    if (!N64EncUnaryGroup(&a->Text, &sink, 0, ext, op))
    {
        N64aDiagAt(a->Diag, line, "unary instruction",
                   "requires a register or memory operand");
        return false;
    }
    return true;
}

static bool EmitUnary(N64aAsm *a, N64Lex *lx, uint8_t ext, const char *mn,
                      uint32_t line)
{
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, a, ops, 1, &n) || n != 1)
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
    N64RelocSink sink;
    SinkInit(&sink, a);

    if (!ParseOperands(lx, a, ops, 1, &n) || n != 1 ||
        !N64EncIncDec(&a->Text, &sink, 0, &ops[0], is_inc))
    {
        N64aDiagAt(a->Diag, line,
                   is_inc ? "inc" : "dec",
                   "requires a register or memory operand");
        return false;
    }
    return true;
}

static bool EmitShift(N64aAsm *a, N64Lex *lx, uint8_t ext, uint32_t line)
{
    N64Operand ops[2];
    unsigned n;
    N64RelocSink sink;
    SinkInit(&sink, a);

    if (!ParseOperands(lx, a, ops, 2, &n) || n != 2 ||
        (ops[0].Kind != N64_OP_REG && ops[0].Kind != N64_OP_MEM))
    {
        N64aDiagAt(a->Diag, line,
                   "shift requires register or memory destination", NULL);
        return false;
    }
    if (ops[1].Kind == N64_OP_REG && ops[1].Reg.Index == 1 &&
        ops[1].Reg.Size == N64_SZ8)
    {
        return N64EncShiftCl(&a->Text, &sink, 0, ext, &ops[0]);
    }
    if (ops[1].Kind == N64_OP_IMM && ops[1].Sym[0] == '\0')
    {
        if (N64EncShiftImm(&a->Text, &sink, 0, ext, &ops[0], ops[1].Imm))
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
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[1];
    unsigned n;

    if (!ParseOperands(lx, a, ops, 1, &n) || n != 1)
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
    if (ops[0].Kind == N64_OP_MEM)
    {
        if (N64EncPushMem(&a->Text, &sink, 0, &ops[0].Mem))
        {
            return true;
        }
        N64aDiagAt(a->Diag, line, "push memory unsupported", NULL);
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

    if (!ParseOperands(lx, a, ops, 1, &n) || n != 1)
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

    if (!ParseOperands(lx, a, ops, 1, &n) || n != 1)
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

    if (!ParseOperands(lx, a, ops, 1, &n) || n != 1)
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

    if (!ParseOperands(lx, a, ops, 1, &n) || n != 1 ||
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
    N64RelocSink sink;
    SinkInit(&sink, a);

    if (!ParseOperands(lx, a, ops, 1, &n) || n != 1)
    {
        N64aDiagAt(a->Diag, line, "setcc needs an operand", NULL);
        return false;
    }
    if (N64EncSetcc(&a->Text, &sink, 0, cc, &ops[0]))
    {
        return true;
    }
    N64aDiagAt(a->Diag, line,
               "setcc requires an 8-bit register or memory destination",
               NULL);
    return false;
}

static bool EmitMovx(N64aAsm *a, N64Lex *lx, bool is_signed, uint32_t line)
{
    N64RelocSink sink;
    SinkInit(&sink, a);
    N64Operand ops[2];
    unsigned n;

    if (!ParseOperands(lx, a, ops, 2, &n) || n != 2 ||
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
                    case 'a': c = '\a'; break;
                    case 'b': c = '\b'; break;
                    case 'f': c = '\f'; break;
                    case 'v': c = '\v'; break;
                    case '0': c = '\0'; break;
                    case '\\': c = '\\'; break;
                    case '"': c = '"'; break;
                    case '\'': c = '\''; break;
                    case 'x':
                    {
                        unsigned int hv = 0;
                        unsigned int n = 0;
                        while (i + 1U < lx->Cur.StrLen && n < 2)
                        {
                            char h = p[i + 1U];
                            uint64_t v;
                            if (h >= '0' && h <= '9') v = (uint64_t)(h - '0');
                            else if (h >= 'a' && h <= 'f')
                                v = (uint64_t)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F')
                                v = (uint64_t)(h - 'A' + 10);
                            else break;
                            hv = (hv << 4) | (unsigned int)v;
                            i++;
                            n++;
                        }
                        c = (char)hv;
                        break;
                    }
                    case 'o':
                    {
                        unsigned int ov = 0;
                        unsigned int n = 0;
                        while (i + 1U < lx->Cur.StrLen && n < 3)
                        {
                            char h = p[i + 1U];
                            if (h < '0' || h > '7') break;
                            ov = (ov << 3) | (unsigned int)(h - '0');
                            i++;
                            n++;
                        }
                        c = (char)ov;
                        break;
                    }
                    default: c = e; break;
                }
            }
            N64BufByte(&a->Data, (uint8_t)c);
        }
        N64LexAdvance(lx);
        return true;
    }

    /* numeric / expression item: if it is a bare symbol (not an equ), use
     * the symbolic path so that 'dd label' emits a relocation.  Anything
     * else goes through the full constant-expression grammar.            */
    if (lx->Cur.Kind == N64_T_IDENT && lx->Cur.Text[0] != '$' &&
        N64aAsmFindConst(a, lx->Cur.Text) == NULL)
    {
        N64Expr e;
        if (!ParseExpr(lx, a, &e))
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
        return true;
    }
    {
        uint64_t v;
        if (!ParseConstExpr(lx, a, &v))
        {
            return false;
        }
        switch (size)
        {
            case N64_SZ8:  N64BufByte(&a->Data, (uint8_t)v);  break;
            case N64_SZ16: N64BufU16(&a->Data, (uint16_t)v);  break;
            case N64_SZ32: N64BufU32(&a->Data, (uint32_t)v);  break;
            default:       N64BufU64(&a->Data, v);            break;
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

    if (AtEol(lx))
    {
        N64aDiagAt(a->Diag, line, "resX requires a count", NULL);
        return false;
    }
    if (!ParseConstExpr(lx, a, &count))
    {
        return false;
    }
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
        if (!ParseOperands(lx, a, ops, 3, &n))
        {
            return false;
        }
        if (n == 1)
        {
            return EmitUnaryOp(a, &ops[0], 5, line);   /* F7 /5 */
        }
        if (n == 2 && ops[0].Kind == N64_OP_REG)
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

    if (N64aStrCmp(mn, "xchg") == 0)
    {
        N64Operand ops[2];
        unsigned n;
        if (!ParseOperands(lx, a, ops, 2, &n) || n != 2 ||
            !N64EncXchg(&a->Text, &sink, 0, &ops[0], &ops[1]))
        {
            N64aDiagAt(a->Diag, line,
                       "xchg requires two same-sized operands "
                       "(reg/reg or reg/mem)", NULL);
            return false;
        }
        return true;
    }

    if (N64aStrCmp(mn, "bsf") == 0 || N64aStrCmp(mn, "bsr") == 0)
    {
        N64Operand ops[2];
        unsigned n;
        if (!ParseOperands(lx, a, ops, 2, &n) || n != 2 ||
            !N64EncBsfBsr(&a->Text, &sink, 0, mn[2] == 'f',
                          &ops[0], &ops[1]))
        {
            N64aDiagAt(a->Diag, line, mn,
                       "requires 'dst reg, src reg/mem' of equal size");
            return false;
        }
        return true;
    }

    if (N64aStrCmp(mn, "bt") == 0 || N64aStrCmp(mn, "bts") == 0 ||
        N64aStrCmp(mn, "btr") == 0 || N64aStrCmp(mn, "btc") == 0)
    {
        N64Operand ops[2];
        unsigned n;
        uint8_t x = mn[2] == 't' ? 0U :
                    mn[2] == 's' ? 1U :
                    mn[2] == 'r' ? 2U : 3U;
        if (!ParseOperands(lx, a, ops, 2, &n) || n != 2 ||
            !N64EncBt(&a->Text, &sink, 0, x, &ops[0], &ops[1]))
        {
            N64aDiagAt(a->Diag, line, mn,
                       "requires 'r/m, r' or 'r/m, imm' (bit index)");
            return false;
        }
        return true;
    }

    /* cmov<cc> dst reg, src r/m */
    if (mn[0] == 'c' && mn[1] == 'm' && mn[2] == 'o' && mn[3] == 'v' &&
        N64CondFind(&mn[4], &cc))
    {
        N64Operand ops[2];
        unsigned n;
        if (!ParseOperands(lx, a, ops, 2, &n) || n != 2 ||
            !N64EncCmovcc(&a->Text, &sink, 0, cc, &ops[0], &ops[1]))
        {
            N64aDiagAt(a->Diag, line, mn,
                       "requires 'dst reg, src reg/mem' (not 8-bit)");
            return false;
        }
        return true;
    }

    if (N64aStrCmp(mn, "ret") == 0)
    {
        N64Operand ops[1];
        unsigned n = 0;
        if (!AtEol(lx))
        {
            if (!ParseOperands(lx, a, ops, 1, &n))
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

static bool ParseStatement(N64Lex *lx, N64aAsm *a);

/* true for keywords that may legally follow a colon-less label         */
static bool IsDataMnemonic(const char *name)
{
    char l[16];
    LowerCopy(l, name);
    return N64aStrCmp(l, "db") == 0 || N64aStrCmp(l, "byte") == 0 ||
           N64aStrCmp(l, "dw") == 0 || N64aStrCmp(l, "word") == 0 ||
           N64aStrCmp(l, "dd") == 0 || N64aStrCmp(l, "dq") == 0 ||
           N64aStrCmp(l, "resb") == 0 || N64aStrCmp(l, "resw") == 0 ||
           N64aStrCmp(l, "resd") == 0 || N64aStrCmp(l, "resq") == 0 ||
           N64aStrCmp(l, "times") == 0 || N64aStrCmp(l, "equ") == 0 ||
           N64aStrCmp(l, "align") == 0 || N64aStrCmp(l, "alignb") == 0;
}

/* 'times <count> <statement>' — replays the remainder of the line by
 * rewinding the lexer and re-parsing the statement <count> times.       */
static bool EmitTimes(N64Lex *lx, N64aAsm *a, uint32_t line)
{
    uint64_t stmt_pos;
    uint32_t stmt_line;
    uint64_t count;
    uint64_t i;

    N64LexAdvance(lx); /* consume 'times' */
    if (!ParseConstExpr(lx, a, &count))
    {
        return false;
    }
    if (AtEol(lx))
    {
        N64aDiagAt(a->Diag, line, "'times' needs a statement to repeat",
                   NULL);
        return false;
    }
    if (count > 100000000U)
    {
        N64aDiagAt(a->Diag, line, "times count too large", NULL);
        return false;
    }
    stmt_pos = lx->Cur.Start; /* start of the first statement token    */
    stmt_line = lx->Cur.Line;

    for (i = 0; i < count; i++)
    {
        lx->Pos = stmt_pos;
        lx->Line = stmt_line;
        lx->CurValid = false;
        lx->PeekValid = false;
        if (!N64LexAdvance(lx))            /* Cur = first stmt token */
        {
            return false;
        }
        if (!ParseStatement(lx, a))        /* Cur = EOL on success */
        {
            return false;
        }
        if (!AtEol(lx))
        {
            lx->CurValid = false;
            lx->PeekValid = false;
        }
    }
    if (count == 0U)
    {
        while (!AtEol(lx))
        {
            N64LexAdvance(lx);
        }
    }
    return !a->Diag->Failed;
}

/* 'align N [, db fill]' — pads the current section to a multiple of N. */
static bool EmitAlign(N64Lex *lx, N64aAsm *a, uint32_t line)
{
    uint64_t n;
    uint64_t fill;
    uint64_t off;
    uint64_t pad;
    uint64_t i;
    N64Buf *buf;

    N64LexAdvance(lx); /* consume 'align' */
    if (!ParseConstExpr(lx, a, &n) || n == 0U || (n & (n - 1U)) != 0U)
    {
        N64aDiagAt(a->Diag, line, "align needs a nonzero power of two",
                   NULL);
        return false;
    }
    fill = a->Section == N64_SEC_TEXT ? 0x90U : 0x00U;
    if (lx->Cur.Kind == N64_T_COMMA)
    {
        N64LexAdvance(lx);
        if (lx->Cur.Kind == N64_T_IDENT &&
            (N64aStrCmp(lx->Cur.Text, "db") == 0 ||
             N64aStrCmp(lx->Cur.Text, "byte") == 0))
        {
            N64LexAdvance(lx);
        }
        if (lx->Cur.Kind != N64_T_NUMBER && lx->Cur.Kind != N64_T_CHAR)
        {
            ParseError(lx, &lx->Cur, "align fill expects a byte value",
                       NULL);
            return false;
        }
        fill = lx->Cur.Num & 0xFFU;
        N64LexAdvance(lx);
    }
    if (!AtEol(lx))
    {
        ParseError(lx, &lx->Cur, "unexpected token after align", NULL);
        return false;
    }
    buf = a->Section == N64_SEC_TEXT ? &a->Text : &a->Data;
    off = buf->Size;
    pad = (n - (off % n)) % n;
    for (i = 0; i < pad; i++)
    {
        N64BufByte(buf, (uint8_t)fill);
    }
    return true;
}

static bool ParseStatement(N64Lex *lx, N64aAsm *a)
{
    uint32_t line = lx->Cur.Line;

    if (lx->Cur.Kind == N64_T_IDENT)
    {
        /* 'name equ <const-expr>' — detected by peeking the next token */
        if (N64LexPeekIs(lx, N64_T_IDENT))
        {
            char peek_lower[16];
            LowerCopy(peek_lower, lx->Peek.Text);
            if (N64aStrCmp(peek_lower, "equ") == 0)
            {
            char name[64];
            uint64_t val;
            N64aStrNCpy(name, lx->Cur.Text, sizeof(name));
            N64LexAdvance(lx); /* name */
            N64LexAdvance(lx); /* equ */
            if (!ParseConstExpr(lx, a, &val))
            {
                return false;
            }
            if (!AtEol(lx))
            {
                ParseError(lx, &lx->Cur, "unexpected token after equ",
                           NULL);
                return false;
            }
            {
                N64Sym *cs = N64aAsmFindSym(a, name);
                if (cs != NULL && (cs->Kind != 0U || cs->IsGlobal))
                {
                    N64aDiagAt(a->Diag, line,
                               "constant name collides with a symbol",
                               name);
                    return false;
                }
            }
            return N64aAsmDefineConst(a, name, val);
            }
        }

        /* label definition: 'name:' or NASM-style 'name db ...'
           (skip when the token is itself an instruction mnemonic so
           that 'mov byte [mem], imm' is not read as label 'mov')     */
        if (N64LexPeekIs(lx, N64_T_COLON) ||
            (N64LexPeekIs(lx, N64_T_IDENT) &&
             IsDataMnemonic(lx->Peek.Text) &&
             !IsInstructionMnemonic(lx->Cur.Text)))
        {
            uint32_t kind;
            uint32_t section = a->Section == N64_SEC_TEXT ?
                               N64_SEC_TEXT : N64_SEC_DATA;
            kind = a->Section == N64_SEC_TEXT ? N64O_SYM_FUNC :
                                                N64O_SYM_DATA;
            if (N64aAsmFindConst(a, lx->Cur.Text) != NULL)
            {
                N64aDiagAt(a->Diag, line,
                           "label collides with a constant", lx->Cur.Text);
                return false;
            }
            if (!N64aAsmDefineSym(a, lx->Cur.Text, kind, section,
                                  a->Section == N64_SEC_TEXT ?
                                  a->Text.Size : a->Data.Size))
            {
                return false;
            }
            N64LexAdvance(lx); /* ident */
            if (lx->Cur.Kind == N64_T_COLON)
            {
                N64LexAdvance(lx); /* colon */
            }
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

        if (N64aStrCmp(lower, "times") == 0)
        {
            return EmitTimes(lx, a, line);
        }

        if (N64aStrCmp(lower, "align") == 0 ||
            N64aStrCmp(lower, "alignb") == 0)
        {
            return EmitAlign(lx, a, line);
        }

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
                       "'equ' must be written as 'name equ expression'",
                       NULL);
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
