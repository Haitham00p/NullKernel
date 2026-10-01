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

static bool IsOctDigit(char c)
{
    return c >= '0' && c <= '7';
}

static bool IsBinDigit(char c)
{
    return c == '0' || c == '1';
}

static bool IsHexDigit(char c)
{
    return IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
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
    t->Start = lx->Pos;

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
        uint64_t p;
        uint64_t run_end;
        bool first_is_zero = (c == '0');

        /* 0x / 0X hex */
        if (c == '0' && lx->Pos + 1U < lx->Len &&
            (s[lx->Pos + 1U] == 'x' || s[lx->Pos + 1U] == 'X'))
        {
            lx->Pos += 2;
            while (lx->Pos < lx->Len)
            {
                if (s[lx->Pos] == '_')
                {
                    lx->Pos++;
                    continue;
                }
                if (!IsHexDigit(s[lx->Pos]))
                {
                    break;
                }
                v = v * 16ULL + HexVal(s[lx->Pos]);
                lx->Pos++;
            }
            if (lx->Pos < lx->Len && IsIdentChar(s[lx->Pos]))
            {
                LexError(lx, "invalid number", NULL);
                lx->Pos++;
            }
            t->Num = v;
            t->Kind = N64_T_NUMBER;
            return;
        }

        /* 0b / 0B binary */
        if (c == '0' && lx->Pos + 1U < lx->Len &&
            (s[lx->Pos + 1U] == 'b' || s[lx->Pos + 1U] == 'B'))
        {
            lx->Pos += 2;
            while (lx->Pos < lx->Len)
            {
                if (s[lx->Pos] == '_')
                {
                    lx->Pos++;
                    continue;
                }
                if (!IsBinDigit(s[lx->Pos]))
                {
                    break;
                }
                v = v * 2ULL + (uint64_t)(s[lx->Pos] - '0');
                lx->Pos++;
            }
            if (lx->Pos < lx->Len && IsIdentChar(s[lx->Pos]))
            {
                LexError(lx, "invalid number", NULL);
                lx->Pos++;
            }
            t->Num = v;
            t->Kind = N64_T_NUMBER;
            return;
        }

        /* 0o / 0q octal */
        if (c == '0' && lx->Pos + 1U < lx->Len &&
            (s[lx->Pos + 1U] == 'o' || s[lx->Pos + 1U] == 'O' ||
             s[lx->Pos + 1U] == 'q' || s[lx->Pos + 1U] == 'Q'))
        {
            lx->Pos += 2;
            while (lx->Pos < lx->Len)
            {
                if (s[lx->Pos] == '_')
                {
                    lx->Pos++;
                    continue;
                }
                if (!IsOctDigit(s[lx->Pos]))
                {
                    break;
                }
                v = v * 8ULL + (uint64_t)(s[lx->Pos] - '0');
                lx->Pos++;
            }
            if (lx->Pos < lx->Len && IsIdentChar(s[lx->Pos]))
            {
                LexError(lx, "invalid number", NULL);
                lx->Pos++;
            }
            t->Num = v;
            t->Kind = N64_T_NUMBER;
            return;
        }

        /* decimal / trailing-h hex / leading-0 octal: scan the full run
         * of digits, hex letters and underscores first, then decide.   */
        run_end = lx->Pos;
        while (run_end < lx->Len &&
               (IsDigit(s[run_end]) || IsHexDigit(s[run_end]) ||
                s[run_end] == '_'))
        {
            run_end++;
        }

        if (run_end < lx->Len &&
            (s[run_end] == 'h' || s[run_end] == 'H') &&
            !(run_end + 1U < lx->Len && IsIdentChar(s[run_end + 1U])))
        {
            /* trailing-h hex: 1234h / 0FFh */
            for (p = start; p < run_end; p++)
            {
                if (s[p] == '_')
                {
                    continue;
                }
                v = v * 16ULL + HexVal(s[p]);
            }
            lx->Pos = run_end + 1; /* consume the 'h' */
        }
        else
        {
            bool valid = true;
            for (p = start; p < run_end; p++)
            {
                if (s[p] == '_')
                {
                    continue;
                }
                if (!IsDigit(s[p]))
                {
                    valid = false; /* hex letter without trailing h */
                    break;
                }
            }
            if (!valid)
            {
                LexError(lx, "invalid number", NULL);
            }
            else if (first_is_zero && run_end - start > 1U)
            {
                /* leading-zero octal: 0777 */
                for (p = start + 1U; p < run_end; p++)
                {
                    if (s[p] == '_')
                    {
                        continue;
                    }
                    if (s[p] == '8' || s[p] == '9')
                    {
                        LexError(lx, "invalid octal number", NULL);
                        break;
                    }
                    v = v * 8ULL + (uint64_t)(s[p] - '0');
                }
            }
            else
            {
                for (p = start; p < run_end; p++)
                {
                    if (s[p] == '_')
                    {
                        continue;
                    }
                    v = v * 10ULL + (uint64_t)(s[p] - '0');
                }
            }
            lx->Pos = run_end;
            if (run_end < lx->Len && IsIdentChar(s[run_end]))
            {
                LexError(lx, "invalid number", NULL);
                lx->Pos++;
            }
        }
        t->Num = v;
        t->Kind = N64_T_NUMBER;
        return;
    }

    switch (c)
    {
        case ',': lx->Pos++; t->Kind = N64_T_COMMA;   return;
        case ':': lx->Pos++; t->Kind = N64_T_COLON;   return;
        case '[': lx->Pos++; t->Kind = N64_T_LBRACK;  return;
        case ']': lx->Pos++; t->Kind = N64_T_RBRACK;  return;
        case '+': lx->Pos++; t->Kind = N64_T_PLUS;    return;
        case '-': lx->Pos++; t->Kind = N64_T_MINUS;   return;
        case '*': lx->Pos++; t->Kind = N64_T_STAR;    return;
        case '/': lx->Pos++; t->Kind = N64_T_SLASH;   return;
        case '%': lx->Pos++; t->Kind = N64_T_PERCENT; return;
        case '(': lx->Pos++; t->Kind = N64_T_LPAREN;  return;
        case ')': lx->Pos++; t->Kind = N64_T_RPAREN;  return;
        case '~': lx->Pos++; t->Kind = N64_T_TILDE;   return;
        case '!': lx->Pos++; t->Kind = N64_T_BANG;    return;
        case '<': lx->Pos++; t->Kind = N64_T_LT;      return;
        case '>': lx->Pos++; t->Kind = N64_T_GT;      return;
        case '&': lx->Pos++; t->Kind = N64_T_AMP;     return;
        case '^': lx->Pos++; t->Kind = N64_T_CARET;   return;
        case '|': lx->Pos++; t->Kind = N64_T_PIPE;    return;
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
                case 'n':  t->Num = '\n'; lx->Pos++; break;
                case 't':  t->Num = '\t'; lx->Pos++; break;
                case 'r':  t->Num = '\r'; lx->Pos++; break;
                case 'a':  t->Num = '\a'; lx->Pos++; break;
                case 'b':  t->Num = '\b'; lx->Pos++; break;
                case 'f':  t->Num = '\f'; lx->Pos++; break;
                case 'v':  t->Num = '\v'; lx->Pos++; break;
                case '0':  t->Num = '\0'; lx->Pos++; break;
                case '\\': t->Num = '\\'; lx->Pos++; break;
                case '\'': t->Num = '\''; lx->Pos++; break;
                case '"':  t->Num = '"';  lx->Pos++; break;
                case 'x':
                {
                    unsigned int n = 0;
                    lx->Pos++;
                    while (lx->Pos < lx->Len && n < 2 &&
                           IsHexDigit(s[lx->Pos]))
                    {
                        t->Num = (t->Num << 4) | HexVal(s[lx->Pos]);
                        lx->Pos++;
                        n++;
                    }
                    if (n == 0)
                    {
                        LexError(lx, "\\x needs hex digits in char literal", NULL);
                    }
                    break;
                }
                default:
                    t->Num = (unsigned char)e;
                    lx->Pos++;
                    LexError(lx, "unknown escape in char literal", NULL);
                    break;
            }
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
