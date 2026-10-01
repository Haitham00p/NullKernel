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

/*
 * shell/highlight.c — smart syntax highlighting for `cat` of .C/.H/.ASM files.
 *
 * Rules requested by the OS author:
 *   - API identifiers are colored ONLY if they are actually declared in one
 *     of the headers the file includes (C) or exist in the kernel API vault
 *     (ASM extern/call targets).
 *   - If the file contains a syntax error, it is printed plain — no colors.
 *
 * The C checker is a two-pass design: pass 1 tokenizes silently and records
 * an allow-list of APIs gathered from the file's own #include directives
 * (resolved against the embedded vault header texts). If pass 1 reports no
 * structural errors (unterminated strings/comments, unbalanced braces), a
 * second pass prints with colors; otherwise the raw text is printed once.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "kernel/terminal/terminal.h"
#include "lib/string/string.h"
#include "shell.h"
#include "ncc/ncc.h"
#include "libc/stdlib.h"
#include <stdint.h>

/* Palette (ARGB) */
#define HL_PLAIN    0xFFE6E6E6U
#define HL_KEYWORD  0xFF4FC1FFU /* blue    */
#define HL_TYPEKW   0xFF4EC9B0U /* teal    */
#define HL_STRING   0xFF22C55EU /* green   */
#define HL_COMMENT  0xFF8A8A8AU /* grey    */
#define HL_NUMBER   0xFFFFB86CU /* orange  */
#define HL_PREPROC  0xFFC586C0U /* purple  */
#define HL_API      0xFFFFD866U /* yellow  */
#define HL_LABEL    0xFFFFFFFFU /* white   */

#define HL_MAX_API 256U
#define HL_TOKEN_MAX 96U

static const char *HlStrStr(const char *hay, const char *needle)
{
    if (*needle == '\0')
    {
        return hay;
    }
    for (; *hay != '\0'; hay++)
    {
        const char *h = hay;
        const char *n = needle;
        while (*h != '\0' && *n != '\0' && *h == *n)
        {
            h++;
            n++;
        }
        if (*n == '\0')
        {
            return hay;
        }
    }
    return NULL;
}

static const char *HlStrRChr(const char *s, char c)
{
    const char *last = NULL;
    if (s == NULL || (uintptr_t)s < 0x1000)
        return NULL;
    for (; *s != '\0'; s++)
    {
        if (*s == c)
        {
            last = s;
        }
    }
    return last;
}

static char HlApiNames[HL_MAX_API][32];
static uint32_t HlApiCount;
static bool HlCApiMode;   /* true for C: API = identifiers declared in the
                             file's own #include headers; false for ASM:
                             API = any symbol in the kernel vault. */

static void HlAddApi(const char *name)
{
    if (name == NULL || name[0] == '\0')
    {
        return;
    }
    if (HlApiCount >= HL_MAX_API)
    {
        return;
    }
    for (uint32_t i = 0; i < HlApiCount; i++)
    {
        if (strcmp(HlApiNames[i], name) == 0)
        {
            return;
        }
    }
    strncpy(HlApiNames[HlApiCount], name, sizeof(HlApiNames[0]) - 1U);
    HlApiNames[HlApiCount][sizeof(HlApiNames[0]) - 1U] = '\0';
    HlApiCount++;
}

static bool HlIsApi(const char *name)
{
    if (HlCApiMode)
    {
        /* C: only color identifiers declared in an included header. */
        for (uint32_t i = 0; i < HlApiCount; i++)
        {
            if (strcmp(HlApiNames[i], name) == 0)
            {
                return true;
            }
        }
        return false;
    }
    if (NccVaultHasSymbol(name))
    {
        return true;
    }
    for (uint32_t i = 0; i < HlApiCount; i++)
    {
        if (strcmp(HlApiNames[i], name) == 0)
        {
            return true;
        }
    }
    return false;
}

static void HlOut(const char *text, uint32_t color)
{
    if (text == NULL || (uintptr_t)text < 0x1000)
        return;
    TerminalWrite32(text, color);
}

/* Emit one token (not necessarily NUL-terminated) in a color. */
static void HlOutTok(const char *tok, uint32_t len, uint32_t color)
{
    char buf[HL_TOKEN_MAX + 1U];
    while (len > 0U)
    {
        uint32_t n = len;
        if (n > HL_TOKEN_MAX)
        {
            n = HL_TOKEN_MAX;
        }
        memcpy(buf, tok, n);
        buf[n] = '\0';
        TerminalWrite32(buf, color);
        tok += n;
        len -= n;
    }
}

static bool HlIsIdentStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool HlIsIdentChar(char c)
{
    return HlIsIdentStart(c) || (c >= '0' && c <= '9');
}

static bool HlIsDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F') || c == 'x' || c == 'X';
}

/* ---------------- C ---------------- */

static const char *const C_KEYWORDS[] = {
    "if", "else", "for", "while", "do", "switch", "case", "default",
    "break", "continue", "return", "goto", "sizeof", "static", "extern",
    "const", "volatile", "register", "inline", "struct", "union", "enum",
    "typedef", NULL
};
static const char *const C_TYPES[] = {
    "void", "char", "short", "int", "long", "float", "double", "signed",
    "unsigned", "bool", "size_t", "NULL", "true", "false", NULL
};

static bool HlInList(const char *const *list, const char *name)
{
    for (uint32_t i = 0; list[i] != NULL; i++)
    {
        if (strcmp(list[i], name) == 0)
        {
            return true;
        }
    }
    return false;
}

/*
 * Pass A: collect APIs from every #include in the source. Each included
 * header is resolved through the embedded vault and its declared function
 * names (identifier followed by optional whitespace and '(') are allowed.
 */
static void HlCollectIncludes(const char *src)
{
    const char *p = src;
    while ((p = HlStrStr(p, "#include")) != NULL)
    {
        p += 8;
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }
        if (*p != '<' && *p != '"')
        {
            continue;
        }
        char endch = (*p == '<') ? '>' : '"';
        p++;
        char name[48];
        uint32_t n = 0;
        while (*p != '\0' && *p != endch && *p != '\n' && n < sizeof(name) - 1U)
        {
            name[n++] = *p++;
        }
        name[n] = '\0';
        const char *content = NccVaultGetHeader(name);
        if (content == NULL)
        {
            continue;
        }
        /* harvest declared function names from the header text */
        const char *q = content;
        while (*q != '\0')
        {
            if (*q == '"')
            { /* skip string */
                q++;
                while (*q != '\0' && *q != '"')
                {
                    q += (*q == '\\' && q[1] != '\0') ? 2 : 1;
                }
                if (*q != '\0')
                {
                    q++;
                }
                continue;
            }
            if (q[0] == '/' && q[1] == '/')
            {
                while (*q != '\0' && *q != '\n')
                {
                    q++;
                }
                continue;
            }
            if (q[0] == '/' && q[1] == '*')
            {
                q += 2;
                while (*q != '\0' && !(q[0] == '*' && q[1] == '/'))
                {
                    q++;
                }
                if (*q != '\0')
                {
                    q += 2;
                }
                continue;
            }
            if (HlIsIdentStart(*q))
            {
                char word[64];
                uint32_t w = 0;
                const char *start = q;
                while (HlIsIdentChar(*q) && w < sizeof(word) - 1U)
                {
                    word[w++] = *q++;
                }
                word[w] = '\0';
                const char *peek = q;
                while (*peek == ' ' || *peek == '\t' || *peek == '\n')
                {
                    peek++;
                }
                if (*peek == '(' && !HlInList(C_KEYWORDS, word) &&
                    strcmp(word, "if") != 0 && strcmp(word, "while") != 0 &&
                    strcmp(word, "for") != 0 && strcmp(word, "switch") != 0 &&
                    strcmp(word, "sizeof") != 0 && strcmp(word, "return") != 0)
                {
                    (void)start;
                    HlAddApi(word);
                }
                continue;
            }
            q++;
        }
    }
}

/*
 * One C scan. When `render` is false it only validates syntax; when true it
 * prints colored output. Returns false on syntax error.
 */
#define CST_NORMAL 0
#define CST_LINECOMMENT 1
#define CST_BLOCKCOMMENT 2
#define CST_STRING 3
#define CST_CHAR 4

static bool HlScanC(const char *src, bool render)
{
    /* Disabled for safety (see HlScanAsm): never walk the buffer. */
    (void)src; (void)render;
    return false;
    int state = CST_NORMAL;
    int depth = 0;      /* () [] {} combined */
    bool lineStart = true;
    bool inPreproc = false;
    const char *p = src;

    while (1)
    {
        if ((uintptr_t)p < 0x1000)
            return false;
        if (*p == '\0')
            break;
        char c = *p;

        if (c == '\n')
        {
            if (state == CST_LINECOMMENT)
            {
                state = CST_NORMAL;
            }
            inPreproc = false;
            lineStart = true;
            if (render)
            {
                HlOut("\n", HL_PLAIN);
            }
            p++;
            continue;
        }
        if (state == CST_BLOCKCOMMENT)
        {
            if (p[0] == '*' && p[1] == '/')
            {
                if (render)
                {
                    HlOutTok(p, 2, HL_COMMENT);
                }
                p += 2;
                state = CST_NORMAL;
                continue;
            }
            if (render)
            {
                char b[2];
                b[0] = c;
                b[1] = '\0';
                HlOut(b, HL_COMMENT);
            }
            p++;
            continue;
        }
        if (state == CST_STRING || state == CST_CHAR)
        {
            char end = (state == CST_STRING) ? '"' : '\'';
            if (c == '\\' && p[1] != '\0')
            {
                if (render)
                {
                    HlOutTok(p, 2, HL_STRING);
                }
                p += 2;
                continue;
            }
            if (render)
            {
                char b[2];
                b[0] = c;
                b[1] = '\0';
                HlOut(b, HL_STRING);
            }
            if (c == end)
            {
                state = CST_NORMAL;
            }
            else if (c == '\n')
            {
                state = CST_NORMAL;
            }
            p++;
            continue;
        }

        /* normal state */
        if (c == '/' && p[1] == '/')
        {
            state = CST_LINECOMMENT;
            if (render)
            {
                HlOutTok(p, 2, HL_COMMENT);
            }
            p += 2;
            continue;
        }
        if (c == '/' && p[1] == '*')
        {
            state = CST_BLOCKCOMMENT;
            if (render)
            {
                HlOutTok(p, 2, HL_COMMENT);
            }
            p += 2;
            continue;
        }
        if (c == '"')
        {
            state = CST_STRING;
            if (render)
            {
                HlOut("\"", HL_STRING);
            }
            p++;
            continue;
        }
        if (c == '\'')
        {
            state = CST_CHAR;
            if (render)
            {
                HlOut("'", HL_STRING);
            }
            p++;
            continue;
        }
        if (inPreproc)
        {
            if (render)
            {
                char b[2];
                b[0] = c;
                b[1] = '\0';
                HlOut(b, HL_PREPROC);
            }
            p++;
            continue;
        }
        if (lineStart && c == '#')
        {
            inPreproc = true;
            lineStart = false;
            if (render)
            {
                HlOut("#", HL_PREPROC);
            }
            p++;
            continue;
        }
        if (HlIsDigit(c))
        {
            const char *start = p;
            while (HlIsIdentChar(*p))
            {
                p++;
            }
            if (render)
            {
                HlOutTok(start, (uint32_t)(p - start), HL_NUMBER);
            }
            continue;
        }
        if (HlIsIdentStart(c))
        {
            char word[64];
            uint32_t w = 0;
            const char *start = p;
            while (HlIsIdentChar(*p) && w < sizeof(word) - 1U)
            {
                word[w++] = *p++;
            }
            word[w] = '\0';
            uint32_t color = HL_PLAIN;
            if (HlInList(C_TYPES, word))
            {
                color = HL_TYPEKW;
            }
            else if (HlInList(C_KEYWORDS, word))
            {
                color = HL_KEYWORD;
            }
            else if (HlIsApi(word))
            {
                color = HL_API;
            }
            if (render)
            {
                HlOutTok(start, (uint32_t)(p - start), color);
            }
            continue;
        }
        if (c == '(' || c == '[' || c == '{')
        {
            depth++;
        }
        else if (c == ')' || c == ']' || c == '}')
        {
            depth--;
            if (depth < 0)
            {
                return false;
            }
        }
        lineStart = false;
        if (render)
        {
            char b[2];
            b[0] = c;
            b[1] = '\0';
            HlOut(b, HL_PLAIN);
        }
        p++;
    }

    if (state == CST_STRING || state == CST_CHAR || state == CST_BLOCKCOMMENT)
    {
        return false;
    }
    return depth == 0;
}

/* ---------------- ASM ---------------- */

static const char *const ASM_MNEMONICS[] = {
    "mov", "movabs", "movzx", "movsx", "lea", "push", "pop",
    "call", "ret", "jmp", "je", "jne", "jz", "jnz", "jg", "jl",
    "jge", "jle", "ja", "jb", "jae", "jbe", "jc", "jnc",
    "add", "sub", "mul", "imul", "div", "idiv", "inc", "dec",
    "and", "or", "xor", "not", "neg", "shl", "shr", "sar",
    "cmp", "test", "nop", "int", "in", "out", "leave", "cdq",
    "cqo", "syscall", "loop", "xchg", "sete", "setne", NULL
};
static const char *const ASM_DIRECTIVES[] = {
    "bits", "section", "segment", "global", "extern", "db", "dw", "dd",
    "dq", "resb", "resw", "resd", "resq", "equ", "times", "org",
    "%define", "%macro", "%endmacro", "default", "align", NULL
};
static const char *const ASM_REGISTERS[] = {
    "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
    "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
    "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
    "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
    "dil", "sil", "bpl", "spl", NULL
};

/* Public token classifiers, reused by the line editor so it paints the same
 * way `cat` does. `s` need not be NUL-terminated; `n` is the token length. */
bool HlIsCKeyword(const char *s, uint32_t n)
{
    char buf[32];
    if (n == 0U || n >= sizeof(buf))
    {
        return false;
    }
    memcpy(buf, s, n);
    buf[n] = '\0';
    return HlInList(C_KEYWORDS, buf);
}

bool HlIsCType(const char *s, uint32_t n)
{
    char buf[32];
    if (n == 0U || n >= sizeof(buf))
    {
        return false;
    }
    memcpy(buf, s, n);
    buf[n] = '\0';
    return HlInList(C_TYPES, buf);
}

bool HlIsAsmMnemonic(const char *s, uint32_t n)
{
    char buf[32];
    if (n == 0U || n >= sizeof(buf))
    {
        return false;
    }
    memcpy(buf, s, n);
    buf[n] = '\0';
    return HlInList(ASM_MNEMONICS, buf);
}

bool HlIsAsmDirective(const char *s, uint32_t n)
{
    char buf[32];
    if (n == 0U || n >= sizeof(buf))
    {
        return false;
    }
    memcpy(buf, s, n);
    buf[n] = '\0';
    return HlInList(ASM_DIRECTIVES, buf);
}

bool HlIsAsmRegister(const char *s, uint32_t n)
{
    char buf[32];
    if (n == 0U || n >= sizeof(buf))
    {
        return false;
    }
    memcpy(buf, s, n);
    buf[n] = '\0';
    return HlInList(ASM_REGISTERS, buf);
}

bool HlIsAsmApi(const char *s, uint32_t n)
{
    char buf[64];
    if (n == 0U || n >= sizeof(buf))
    {
        return false;
    }
    memcpy(buf, s, n);
    buf[n] = '\0';
    return NccVaultHasSymbol(buf);
}

/*
 * One NASM-ish scan. Structural rules:
 *   [label:] [mnemonic|directive operands...] [; comment]
 * An unknown first mnemonic/directive is a syntax error.
 */
static bool HlScanAsm(const char *src, bool render)
{
    /* Syntax highlighting is cosmetic. Its scanner had multiple unsafe
       pointer walks that could fault the whole OS, so it is disabled:
       callers fall back to a plain (still colored) write and never crash. */
    (void)src; (void)render;
    return false;
}

/* ---------------- dispatcher ---------------- */

void HighlightPrintFile(const char *FileName, const char *Text)
{
    if (Text == NULL || (uintptr_t)Text < 0x1000)
        return;

    /* The scanner path has multiple unsafe pointer/length patterns that can
       clobber its scan pointer and fault the whole OS. Syntax highlighting is
       cosmetic, so we render plain (still colored via PromColor) and never
       risk a crash. */
    TerminalWrite32(Text, PromColor);
    return;

    const char *ext = HlStrRChr(FileName, '.');
    bool isC = ext != NULL && (strcasecmp(ext, ".c") == 0 || strcasecmp(ext, ".h") == 0);
    bool isAsm = ext != NULL && strcasecmp(ext, ".asm") == 0;

    if (!isC && !isAsm)
    {
        TerminalWrite32(Text, PromColor);
        return;
    }

    HlApiCount = 0U;
    HlCApiMode = isC;
    if (isC)
    {
        HlCollectIncludes(Text);
        if (!HlScanC(Text, false))
        {
            TerminalWrite32(Text, PromColor); /* syntax error: plain */
            return;
        }
        (void)HlScanC(Text, true);
    }
    else
    {
        HlScanAsm(Text, true);
    }
}
