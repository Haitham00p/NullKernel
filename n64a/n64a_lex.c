/* n64a — lexer */
#include "n64a_lex.h"

void N64aDiagInit(N64aDiag *d, const char *file)
{
    d->Failed = false;
    d->Line = 0;
    N64aStrNCpy(d->File, file, sizeof(d->File));
    d->Message[0] = '\0';
}

void N64aDiagAt(N64aDiag *d, uint32_t line, const char *msg, const char *detail)
{
    if (d->Failed)
    {
        return; /* first error wins */
    }
    d->Failed = true;
    d->Line = line;
    N64aStrCpy(d->Message, msg);
    if (detail != NULL && d->Line != 0xFFFFFFFFU)
    {
        unsigned int i = 0;
        while (d->Message[i] != '\0' && i < sizeof(d->Message) - 4U) i++;
        if (i < sizeof(d->Message) - 2U)
        {
            d->Message[i++] = ':';
            d->Message[i++] = ' ';
            while (*detail != '\0' && i < sizeof(d->Message) - 1U)
            {
                d->Message[i++] = *detail++;
            }
            d->Message[i] = '\0';
        }
    }
}

static bool IsIdentStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
           c == '.' || c == '$';
}

static bool IsIdentChar(char c)
{
    return IsIdentStart(c) || (c >= '0' && c <= '9');
}

static bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

static uint64_t HexVal(char c)
{
    if (c >= '0' && c <= '9') return (uint64_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint64_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint64_t)(c - 'A' + 10);
    return 0;
}

void N64LexInit(N64Lex *lx, const char *file, const char *src,
                unsigned long long len, N64aDiag *diag)
{
    lx->File = file;
    lx->Src = src;
    lx->Len = len;
    lx->Pos = 0;
    lx->Line = 1;
    lx->CurValid = false;
    lx->PeekValid = false;
    lx->Diag = diag;
}

static void LexError(N64Lex *lx, const char *msg, const char *detail)
{
    N64aDiagAt(lx->Diag, lx->Line, msg, detail);
}

/* Scans one raw token starting at lx->Pos.  Newlines produce N64_T_EOL. */
static void ScanOne(N64Lex *lx, N64Tok *t)
{
    const char *s = lx->Src;
    char c;

    /* skip spaces, tabs and comments (';' to end of line) */
    for (;;)
    {
        c = s[lx->Pos];
        if (c == ' ' || c == '\t' || c == '\r')
        {
            lx->Pos++;
            continue;
        }
        if (c == ';')
        {
            while (lx->Pos < lx->Len && s[lx->Pos] != '\n') lx->Pos++;
            continue;
        }
        break;
    }

    t->Num = 0;
    t->Text[0] = '\0';
    t->Str = NULL;
    t->StrLen = 0;
    t->Line = lx->Line;

    if (lx->Pos >= lx->Len)
    {
        t->Kind = N64_T_EOF;
        return;
    }

    c = s[lx->Pos];

    if (c == '\n')
    {
        lx->Pos++;
        t->Kind = N64_T_EOL;
        return;
    }

    if (IsIdentStart(c))
    {
        uint32_t n = 0;
        while (lx->Pos < lx->Len && IsIdentChar(s[lx->Pos]))
        {
            if (n < sizeof(t->Text) - 1U)
            {
                t->Text[n++] = s[lx->Pos];
            }
            lx->Pos++;
        }
        t->Text[n] = '\0';
        t->Kind = N64_T_IDENT;
        return;
    }

    if (IsDigit(c))
    {
        uint64_t v = 0;
        uint64_t start = lx->Pos;
        if (c == '0' && lx->Pos + 1U < lx->Len &&
            (s[lx->Pos + 1U] == 'x' || s[lx->Pos + 1U] == 'X'))
        {
            lx->Pos += 2;
            while (lx->Pos < lx->Len &&
                   ((s[lx->Pos] >= '0' && s[lx->Pos] <= '9') ||
                    (s[lx->Pos] >= 'a' && s[lx->Pos] <= 'f') ||
                    (s[lx->Pos] >= 'A' && s[lx->Pos] <= 'F')))
            {
                v = v * 16ULL + HexVal(s[lx->Pos]);
                lx->Pos++;
            }
        }
        else if (c == '0' && lx->Pos + 1U < lx->Len &&
                 (s[lx->Pos + 1U] == 'b' || s[lx->Pos + 1U] == 'B') &&
                 !(lx->Pos + 2U < lx->Len && IsIdentChar(s[lx->Pos + 2U])))
        {
            lx->Pos += 2;
            while (lx->Pos < lx->Len &&
                   (s[lx->Pos] == '0' || s[lx->Pos] == '1'))
            {
                v = v * 2ULL + (uint64_t)(s[lx->Pos] - '0');
                lx->Pos++;
            }
        }
        else
        {
            while (lx->Pos < lx->Len && IsDigit(s[lx->Pos]))
            {
                lx->Pos++;
            }
            /* NASM-style trailing h: 1234h / 0FFh */
            if (lx->Pos < lx->Len &&
                (s[lx->Pos] == 'h' || s[lx->Pos] == 'H') &&
                !(lx->Pos + 1U < lx->Len &&
                  IsIdentChar(s[lx->Pos + 1U])))
            {
                uint64_t p;
                for (p = start; p < lx->Pos; p++)
                {
                    v = v * 16ULL + HexVal(s[p]);
                }
                lx->Pos++; /* consume the 'h' */
            }
            else
            {
                uint64_t p;
                for (p = start; p < lx->Pos; p++)
                {
                    v = v * 10ULL + (uint64_t)(s[p] - '0');
                }
            }
        }
        t->Num = v;
        t->Kind = N64_T_NUMBER;
        return;
    }

    switch (c)
    {
        case ',': lx->Pos++; t->Kind = N64_T_COMMA;  return;
        case ':': lx->Pos++; t->Kind = N64_T_COLON;  return;
        case '[': lx->Pos++; t->Kind = N64_T_LBRACK; return;
        case ']': lx->Pos++; t->Kind = N64_T_RBRACK; return;
        case '+': lx->Pos++; t->Kind = N64_T_PLUS;   return;
        case '-': lx->Pos++; t->Kind = N64_T_MINUS;  return;
        case '*': lx->Pos++; t->Kind = N64_T_STAR;   return;
        case '(': lx->Pos++; t->Kind = N64_T_LPAREN; return;
        case ')': lx->Pos++; t->Kind = N64_T_RPAREN; return;
        default:
            break;
    }

    if (c == '"')
    {
        lx->Pos++;
        t->Str = &s[lx->Pos];
        t->StrLen = 0;
        while (lx->Pos < lx->Len && s[lx->Pos] != '"' && s[lx->Pos] != '\n')
        {
            lx->Pos++;
            t->StrLen++;
        }
        if (lx->Pos < lx->Len && s[lx->Pos] == '"')
        {
            lx->Pos++;
        }
        else
        {
            LexError(lx, "unterminated string literal", NULL);
        }
        t->Kind = N64_T_STRING;
        return;
    }

    if (c == '\'')
    {
        lx->Pos++;
        if (lx->Pos < lx->Len && s[lx->Pos] == '\\')
        {
            char e;
            lx->Pos++;
            e = s[lx->Pos];
            switch (e)
            {
                case 'n':  t->Num = '\n'; break;
                case 't':  t->Num = '\t'; break;
                case 'r':  t->Num = '\r'; break;
                case '0':  t->Num = '\0'; break;
                case '\\': t->Num = '\\'; break;
                case '\'': t->Num = '\''; break;
                default:
                    t->Num = (unsigned char)e;
                    LexError(lx, "unknown escape in char literal", NULL);
                    break;
            }
            lx->Pos++;
        }
        else if (lx->Pos < lx->Len && s[lx->Pos] != '\'')
        {
            t->Num = (unsigned char)s[lx->Pos];
            lx->Pos++;
        }
        else
        {
            LexError(lx, "empty character literal", NULL);
        }
        if (lx->Pos < lx->Len && s[lx->Pos] == '\'')
        {
            lx->Pos++;
        }
        else
        {
            LexError(lx, "unterminated character literal", NULL);
        }
        t->Kind = N64_T_CHAR;
        return;
    }

    LexError(lx, "unexpected character", NULL);
    lx->Pos++;
    t->Kind = N64_T_EOL;
}

bool N64LexNext(N64Lex *lx, N64Tok *out)
{
    ScanOne(lx, out);
    if (out->Kind == N64_T_EOL)
    {
        lx->Line++; /* newline consumed: next token is on the next line */
    }
    return !lx->Diag->Failed;
}

bool N64LexAdvance(N64Lex *lx)
{
    if (!lx->CurValid)
    {
        N64LexNext(lx, &lx->Cur);
        lx->CurValid = true;
        return !lx->Diag->Failed;
    }
    if (lx->PeekValid)
    {
        lx->Cur = lx->Peek;
        lx->PeekValid = false;
        return !lx->Diag->Failed;
    }
    return N64LexNext(lx, &lx->Cur);
}

bool N64LexPeekIs(N64Lex *lx, N64TokKind k)
{
    if (lx->Diag->Failed)
    {
        return false;
    }
    if (!lx->PeekValid)
    {
        N64LexNext(lx, &lx->Peek);
        lx->PeekValid = true;
    }
    return lx->Peek.Kind == k;
}
