#include "ncc.h"
#include "n64a/n64a.h"
#include "kernel/debug/debug.h"
#include "ncc/vault_gen.h"
#include "lib/string/string.h"
#include "mm/heap/heap.h"
#include "kernel/terminal/terminal.h"
#include "diskfs.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NCC_BASE             0x100000ULL
#define NCC_HDRSZ            0x200U
#define NCC_MAX_MACROS       256U
#define NCC_MAX_LOCALS       512U
#define NCC_MAX_GLOBALS      512U
#define NCC_MAX_FUNCS        256U
#define NCC_MAX_FIXUPS       8192U
#define NCC_MAX_SCOPES       256U
#define NCC_MAX_INCLUDE_DEPTH 8U
#define NCC_MAX_ARGS         6U
#define NCC_MAX_PARAMS       16U
#define NCC_MAX_LOOP         64U
#define NCC_MAX_STR          512U
#define NCC_MAX_CONSTS       256U
#define NCC_MAX_TYPEDEFS     256U

#define VAR_LOCAL   0
#define VAR_PARAM   1
#define VAR_GLOBAL  2
#define VAR_EXTERN  3

#define MODE_RV     0
#define MODE_LV     1

#define ERR_COLOR   0xFFFF0000U
#define OK_COLOR    0xFF57DB92U

typedef enum
{
    TY_VOID = 0,
    TY_CHAR, TY_UCHAR, TY_SHORT, TY_USHORT,
    TY_INT, TY_UINT, TY_LONG, TY_ULONG,
    TY_PTR, TY_ARR, TY_OPQ
} NccTyKind;

typedef struct NccType
{
    NccTyKind kind;
    int size;
    bool is_unsigned;
    struct NccType *elem;
    int arr_len;
    char name[40];
} NccType;

static NccType g_ty_void   = { TY_VOID,   0, false, NULL, 0, "" };
static NccType g_ty_char   = { TY_CHAR,   1, false, NULL, 0, "" };
static NccType g_ty_uchar  = { TY_UCHAR,  1, true,  NULL, 0, "" };
static NccType g_ty_short  = { TY_SHORT,  2, false, NULL, 0, "" };
static NccType g_ty_ushort = { TY_USHORT, 2, true,  NULL, 0, "" };
static NccType g_ty_int    = { TY_INT,    4, false, NULL, 0, "" };
static NccType g_ty_uint   = { TY_UINT,   4, true,  NULL, 0, "" };
static NccType g_ty_long   = { TY_LONG,   8, false, NULL, 0, "" };
static NccType g_ty_ulong  = { TY_ULONG,  8, true,  NULL, 0, "" };

static const char *g_file;
static uint32_t g_err_count;

typedef struct
{
    uint8_t *data;
    uint32_t size;
    uint32_t cap;
} NccBuf;

static void NccBufInit(NccBuf *b, uint32_t cap)
{
    b->cap = cap;
    b->size = 0;
    b->data = (uint8_t *)kmalloc(cap);
}

static void NccBufReserve(NccBuf *b, uint32_t extra)
{
    if (b->size + extra <= b->cap)
    {
        return;
    }
    uint32_t newcap = b->cap * 2U;
    while (newcap < b->size + extra)
    {
        newcap *= 2U;
    }
    uint8_t *nd = (uint8_t *)kmalloc(newcap);
    memcpy(nd, b->data, b->size);
    kfree(b->data);
    b->data = nd;
    b->cap = newcap;
}

static void NccBufPut(NccBuf *b, uint8_t byte)
{
    NccBufReserve(b, 1);
    b->data[b->size++] = byte;
}

static void NccBufBytes(NccBuf *b, const void *src, uint32_t n)
{
    NccBufReserve(b, n);
    memcpy(b->data + b->size, src, n);
    b->size += n;
}

static void NccBufStr(NccBuf *b, const char *s)
{
    NccBufBytes(b, s, (uint32_t)strlen(s));
}

static void NccItoa(uint64_t v, char *out)
{
    char tmp[24];
    uint32_t n = 0;
    do
    {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v != 0);
    for (uint32_t i = 0; i < n; i++)
    {
        out[i] = tmp[n - 1U - i];
    }
    out[n] = '\0';
}

static void NccErrorAt(const char *msg, uint32_t line)
{
    char buf[16];
    g_err_count++;
    if (line == 0)
    {
        line = 1;
    }
    TerminalWrite32("ncc: ", ERR_COLOR);
    TerminalWrite32(g_file != NULL ? g_file : "?", ERR_COLOR);
    TerminalWrite32(":", ERR_COLOR);
    NccItoa(line, buf);
    TerminalWrite32(buf, ERR_COLOR);
    TerminalWrite32(": error: ", ERR_COLOR);
    TerminalPrintLine32(msg, ERR_COLOR);
}

static void NccError(const char *msg)
{
    NccErrorAt(msg, 1);
}

static bool NccIsIdentStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool NccIsIdentChar(char c)
{
    return NccIsIdentStart(c) || (c >= '0' && c <= '9');
}

static bool NccIsWs(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static bool NccIsHex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static uint64_t NccHexVal(char c)
{
    if (c >= '0' && c <= '9')
    {
        return (uint64_t)(c - '0');
    }
    return (uint64_t)((c & 0x5FU) - 'A' + 10);
}

/* ------------------------------------------------------------------ */
/* builtin headers                                                     */
/* ------------------------------------------------------------------ */

static const char NccBuiltinStdint[] =
    "#ifndef NCC_STDINT_H\n"
    "#define NCC_STDINT_H\n"
    "typedef unsigned char uint8_t;\n"
    "typedef unsigned short uint16_t;\n"
    "typedef unsigned int uint32_t;\n"
    "typedef unsigned long uint64_t;\n"
    "typedef signed char int8_t;\n"
    "typedef short int16_t;\n"
    "typedef int int32_t;\n"
    "typedef long int64_t;\n"
    "typedef unsigned long uintptr_t;\n"
    "typedef long intptr_t;\n"
    "#define UINT8_MAX 0xFFU\n"
    "#define UINT16_MAX 0xFFFFU\n"
    "#define UINT32_MAX 0xFFFFFFFFU\n"
    "#define INT32_MAX 0x7FFFFFFF\n"
    "#define INT32_MIN (-INT32_MAX - 1)\n"
    "#define UINT64_MAX 0xFFFFFFFFFFFFFFFFUL\n"
    "#define INT64_MAX 0x7FFFFFFFFFFFFFFFL\n"
    "#define INT64_MIN (-INT64_MAX - 1)\n"
    "#endif\n";

static const char NccBuiltinStddef[] =
    "#ifndef NCC_STDDEF_H\n"
    "#define NCC_STDDEF_H\n"
    "typedef unsigned long size_t;\n"
    "typedef long ptrdiff_t;\n"
    "#define NULL ((void *)0)\n"
    "#endif\n";

static const char NccBuiltinStdbool[] =
    "#ifndef NCC_STDBOOL_H\n"
    "#define NCC_STDBOOL_H\n"
    "typedef unsigned char bool;\n"
    "#define true 1\n"
    "#define false 0\n"
    "#endif\n";

static const char *NccFindBuiltinHeader(const char *name)
{
    if (strcmp(name, "stdint.h") == 0) return NccBuiltinStdint;
    if (strcmp(name, "stddef.h") == 0) return NccBuiltinStddef;
    if (strcmp(name, "stdbool.h") == 0) return NccBuiltinStdbool;
    return NULL;
}

static const char *NccVaultFindHeader(const char *name)
{
    const char *base = name;
    for (const char *p = name; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            base = p + 1;
        }
    }
    for (uint32_t i = 0; i < NccHeaderCount; i++)
    {
        if (strcmp(NccHeaders[i].Path, name) == 0 ||
            strcmp(NccHeaders[i].Name, name) == 0 ||
            strcmp(NccHeaders[i].Name, base) == 0)
        {
            return NccHeaders[i].Content;
        }
    }
    for (uint32_t i = 0; i < NccHeaderCount; i++)
    {
        if (strcasecmp(NccHeaders[i].Name, base) == 0)
        {
            return NccHeaders[i].Content;
        }
    }
    if (base[0] != 'k')
    {
        char candidate[64];
        candidate[0] = 'k';
        strncpy(candidate + 1, base, sizeof(candidate) - 2U);
        candidate[sizeof(candidate) - 1U] = '\0';
        for (uint32_t i = 0; i < NccHeaderCount; i++)
        {
            if (strcasecmp(NccHeaders[i].Name, candidate) == 0)
            {
                return NccHeaders[i].Content;
            }
        }
    }
    return NULL;
}

/* Public accessors for the shell syntax highlighter. */
const char *NccVaultGetHeader(const char *name)
{
    const char *content = NccVaultFindHeader(name);
    if (content == NULL)
    {
        content = NccFindBuiltinHeader(name);
    }
    return content;
}

bool NccVaultHasSymbol(const char *name)
{
    for (uint32_t i = 0; i < NccSymbolCount; i++)
    {
        if (strcmp(NccSymbols[i].Name, name) == 0)
        {
            return true;
        }
    }
    return false;
}

/*
 * Loads a header from the FAT32 working directory.
 * One scratch buffer per nesting depth keeps nested includes from
 * clobbering each other (the preprocessor processes an include to
 * completion before moving on to the next).
 */
#define NCC_DISK_HEADER_MAX 2048U

static char
NccDiskHeaders[NCC_MAX_INCLUDE_DEPTH + 1U][NCC_DISK_HEADER_MAX];

static const char *NccLoadDiskHeader(const char *name, uint32_t depth)
{
    uint32_t idx = depth % (NCC_MAX_INCLUDE_DEPTH + 1U);
    char *buf = NccDiskHeaders[idx];
    uint32_t bytes = 0;

    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        return NULL;
    }
    if (!DiskFsRead(name, buf, NCC_DISK_HEADER_MAX - 1U, &bytes))
    {
        return NULL;
    }
    if (bytes >= NCC_DISK_HEADER_MAX - 1U)
    {
        return NULL; /* header too large to inline safely */
    }
    buf[bytes] = '\0';
    return buf;
}

/* ------------------------------------------------------------------ */
/* preprocessor                                                        */
/* ------------------------------------------------------------------ */

typedef struct
{
    char name[64];
    char repl[512];
} NccMacro;

static NccMacro g_macros[NCC_MAX_MACROS];
static uint32_t g_macro_count;
static bool g_cond_active[NCC_MAX_INCLUDE_DEPTH * 8U];
static bool g_cond_negated[NCC_MAX_INCLUDE_DEPTH * 8U];
static bool g_cond_eval[NCC_MAX_INCLUDE_DEPTH * 8U];
static uint32_t g_cond_depth;
static bool g_pp_ok;

static int NccMacroFind(const char *name)
{
    for (uint32_t i = 0; i < g_macro_count; i++)
    {
        if (strcmp(g_macros[i].name, name) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

static void NccMacroDefine(const char *name, const char *repl)
{
    int idx = NccMacroFind(name);
    if (idx < 0)
    {
        if (g_macro_count >= NCC_MAX_MACROS)
        {
            return;
        }
        idx = (int)g_macro_count++;
        strncpy(g_macros[idx].name, name, sizeof(g_macros[idx].name) - 1U);
        g_macros[idx].name[sizeof(g_macros[idx].name) - 1U] = '\0';
    }
    strncpy(g_macros[idx].repl, repl, sizeof(g_macros[idx].repl) - 1U);
    g_macros[idx].repl[sizeof(g_macros[idx].repl) - 1U] = '\0';
}

static void NccMacroUndef(const char *name)
{
    int idx = NccMacroFind(name);
    if (idx < 0)
    {
        return;
    }
    g_macro_count--;
    for (uint32_t i = (uint32_t)idx; i < g_macro_count; i++)
    {
        g_macros[i] = g_macros[i + 1];
    }
}

static void NccExpandText(NccBuf *out, const char *line, uint32_t len,
                          uint32_t depth, const char *skip)
{
    const char *p = line;
    const char *end = line + len;
    while (p < end)
    {
        char c = *p;
        if (c == '"' || c == '\'')
        {
            char quote = c;
            NccBufPut(out, (uint8_t)c);
            p++;
            while (p < end)
            {
                char d = *p++;
                NccBufPut(out, (uint8_t)d);
                if (d == '\\' && p < end)
                {
                    NccBufPut(out, (uint8_t)*p++);
                }
                else if (d == quote)
                {
                    break;
                }
            }
            continue;
        }
        if (c == '/' && p + 1 < end && p[1] == '*')
        {
            NccBufPut(out, (uint8_t)'/');
            NccBufPut(out, (uint8_t)'*');
            p += 2;
            while (p < end)
            {
                NccBufPut(out, (uint8_t)*p);
                if (p + 1 < end && p[0] == '*' && p[1] == '/')
                {
                    NccBufPut(out, (uint8_t)*++p);
                    p++;
                    break;
                }
                p++;
            }
            continue;
        }
        if (c == '/' && p + 1 < end && p[1] == '/')
        {
            while (p < end)
            {
                NccBufPut(out, (uint8_t)*p++);
            }
            continue;
        }
        if (NccIsIdentStart(c))
        {
            const char *start = p;
            while (p < end && NccIsIdentChar(*p))
            {
                p++;
            }
            uint32_t n = (uint32_t)(p - start);
            char name[64];
            if (n >= sizeof(name))
            {
                n = sizeof(name) - 1U;
            }
            memcpy(name, start, n);
            name[n] = '\0';
            const char *repl = NULL;
            if (depth < 24U && (skip == NULL || strcmp(name, skip) != 0))
            {
                int idx = NccMacroFind(name);
                if (idx >= 0)
                {
                    repl = g_macros[idx].repl;
                }
            }
            if (repl != NULL)
            {
                NccExpandText(out, repl, (uint32_t)strlen(repl), depth + 1, name);
            }
            else
            {
                NccBufBytes(out, start, n);
            }
            continue;
        }
        NccBufPut(out, (uint8_t)c);
        p++;
    }
}

static int NccStrncmp(const char *a, const char *b, uint64_t n);

static bool NccIfExpr(const char *expr)
{
    const char *p = expr;
    bool have = false;
    bool value = false;
    while (*p != '\0')
    {
        while (NccIsWs(*p))
        {
            p++;
        }
        if (*p == '\0')
        {
            break;
        }
        if (p[0] == 'd' && NccStrncmp(p, "defined", 7) == 0 &&
            !NccIsIdentChar(p[7]))
        {
            p += 7;
            while (NccIsWs(*p))
            {
                p++;
            }
            bool paren = false;
            if (*p == '(')
            {
                paren = true;
                p++;
            }
            while (NccIsWs(*p))
            {
                p++;
            }
            const char *s = p;
            while (NccIsIdentChar(*p))
            {
                p++;
            }
            char name[64];
            uint32_t n = (uint32_t)(p - s);
            if (n >= sizeof(name))
            {
                n = sizeof(name) - 1U;
            }
            memcpy(name, s, n);
            name[n] = '\0';
            if (paren)
            {
                while (NccIsWs(*p))
                {
                    p++;
                }
                if (*p == ')')
                {
                    p++;
                }
            }
            have = true;
            value = NccMacroFind(name) >= 0;
            continue;
        }
        if (NccStrncmp(p, "&&", 2) == 0)
        {
            p += 2;
            have = false;
            continue;
        }
        if (NccStrncmp(p, "||", 2) == 0)
        {
            p += 2;
            have = false;
            continue;
        }
        if (*p == '!')
        {
            p++;
            while (NccIsWs(*p))
            {
                p++;
            }
            if (p[0] == 'd' && NccStrncmp(p, "defined", 7) == 0 &&
                !NccIsIdentChar(p[7]))
            {
                p += 7;
                while (NccIsWs(*p))
                {
                    p++;
                }
                bool paren = *p == '(';
                if (paren)
                {
                    p++;
                }
                const char *s = p;
                while (NccIsIdentChar(*p))
                {
                    p++;
                }
                char name[64];
                uint32_t n = (uint32_t)(p - s);
                if (n >= sizeof(name))
                {
                    n = sizeof(name) - 1U;
                }
                memcpy(name, s, n);
                name[n] = '\0';
                if (paren && *p == ')')
                {
                    p++;
                }
                have = true;
                value = NccMacroFind(name) < 0;
                continue;
            }
            have = true;
            value = !value;
            continue;
        }
        if (*p == '0')
        {
            p++;
            have = true;
            value = false;
            continue;
        }
        if (*p == '1')
        {
            p++;
            have = true;
            value = true;
            continue;
        }
        if (*p == '(' || *p == ')')
        {
            p++;
            have = false;
            continue;
        }
        if (NccIsIdentChar(*p))
        {
            while (NccIsIdentChar(*p))
            {
                p++;
            }
            have = true;
            value = false;
            continue;
        }
        p++;
    }
    return have ? value : false;
}

/* Strips C comments (both / * *​/ and //) from one logical line and appends
   the result (with macros expanded) to out.  Updates *in_comment so a block
   comment may span several lines.  Without this, comment text is emitted as
   code and every commented guest header fails to compile. */
static void NccEmitLineStripped(NccBuf *out, const char *start, uint32_t len,
                                bool *in_comment)
{
    static char buf[4096];
    uint32_t n = 0;
    uint32_t i = 0;
    bool emit = !*in_comment;

    while (i < len && n + 1U < sizeof(buf))
    {
        if (*in_comment)
        {
            if (start[i] == '*' && i + 1U < len && start[i + 1U] == '/')
            {
                *in_comment = false;
                emit = true;
                i += 2;
            }
            else
            {
                i++;
            }
            continue;
        }
        if (start[i] == '/' && i + 1U < len && start[i + 1U] == '/')
        {
            break; /* line comment: drop the rest of the line */
        }
        if (start[i] == '/' && i + 1U < len && start[i + 1U] == '*')
        {
            *in_comment = true;
            emit = false;
            i += 2;
            continue;
        }
        if (emit)
        {
            buf[n++] = start[i];
        }
        i++;
    }
    buf[n] = '\0';
    NccExpandText(out, buf, n, 0, NULL);
}

static void NccPpProcess(const char *fname, const char *src, NccBuf *out,
                         uint32_t depth)
{
    if (depth > NCC_MAX_INCLUDE_DEPTH)
    {
        g_file = fname;
        NccErrorAt("include nesting too deep", 0);
        g_pp_ok = false;
        return;
    }
    const char *p = src;
    bool in_comment = false;
    uint32_t line = 1;
    while (*p != '\0')
    {
        const char *start = p;
        while (*p != '\0' && *p != '\n')
        {
            p++;
        }
        uint32_t len = (uint32_t)(p - start);
        while (len > 0 && start[len - 1U] == '\r')
        {
            len--;
        }
        const char *t = start;
        while (t < start + len && NccIsWs(*t))
        {
            t++;
        }
        bool is_directive = !in_comment && t < start + len && *t == '#';
        bool active = g_cond_depth == 0 ? true :
            g_cond_active[g_cond_depth - 1U];
        if (is_directive)
        {
            t++;
            while (t < start + len && NccIsWs(*t))
            {
                t++;
            }
            const char *word = t;
            while (t < start + len && NccIsIdentChar(*t))
            {
                t++;
            }
            uint32_t wlen = (uint32_t)(t - word);
            while (t < start + len && NccIsWs(*t))
            {
                t++;
            }
            char w[16];
            if (wlen >= sizeof(w))
            {
                wlen = sizeof(w) - 1U;
            }
            memcpy(w, word, wlen);
            w[wlen] = '\0';
            const char *rest = t;
            if (strcmp(w, "endif") == 0)
            {
                if (g_cond_depth > 0)
                {
                    g_cond_depth--;
                }
            }
            else if (strcmp(w, "else") == 0)
            {
                if (g_cond_depth > 0 && !g_cond_negated[g_cond_depth - 1U])
                {
                    g_cond_negated[g_cond_depth - 1U] = true;
                    bool parent = g_cond_depth >= 2U ?
                        g_cond_active[g_cond_depth - 2U] : true;
                    g_cond_active[g_cond_depth - 1U] =
                        parent && !g_cond_eval[g_cond_depth - 1U];
                }
            }
            else if (strcmp(w, "ifdef") == 0 || strcmp(w, "ifndef") == 0)
            {
                const char *s = rest;
                while (s < start + len && NccIsWs(*s))
                {
                    s++;
                }
                char name[64];
                uint32_t n = 0;
                while (s < start + len && NccIsIdentChar(*s) &&
                       n < sizeof(name) - 1U)
                {
                    name[n++] = *s++;
                }
                name[n] = '\0';
                bool parent = g_cond_depth == 0 ? true :
                    g_cond_active[g_cond_depth - 1U];
                bool evalv = NccMacroFind(name) >= 0;
                if (strcmp(w, "ifndef") == 0)
                {
                    evalv = !evalv;
                }
                g_cond_eval[g_cond_depth] = evalv;
                g_cond_negated[g_cond_depth] = false;
                g_cond_active[g_cond_depth] = parent && evalv;
                g_cond_depth++;
            }
            else if (strcmp(w, "if") == 0)
            {
                const char *s = rest;
                while (s < start + len && NccIsWs(*s))
                {
                    s++;
                }
                char expr[256];
                uint32_t n = 0;
                while (s < start + len && n < sizeof(expr) - 1U)
                {
                    expr[n++] = *s++;
                }
                expr[n] = '\0';
                bool parent = g_cond_depth == 0 ? true :
                    g_cond_active[g_cond_depth - 1U];
                bool evalv = NccIfExpr(expr);
                g_cond_eval[g_cond_depth] = evalv;
                g_cond_negated[g_cond_depth] = false;
                g_cond_active[g_cond_depth] = parent && evalv;
                g_cond_depth++;
            }
            else if (active)
            {
                if (strcmp(w, "include") == 0)
                {
                    const char *s = rest;
                    while (s < start + len && NccIsWs(*s))
                    {
                        s++;
                    }
                    char incname[128];
                    uint32_t n = 0;
                    if (s < start + len && (*s == '<' || *s == '"'))
                    {
                        char term = *s == '<' ? '>' : '"';
                        s++;
                        while (s < start + len && *s != term &&
                               n < sizeof(incname) - 1U)
                        {
                            incname[n++] = *s++;
                        }
                    }
                    incname[n] = '\0';
                    const char *content = NccFindBuiltinHeader(incname);
                    if (content == NULL)
                    {
                        content = NccVaultFindHeader(incname);
                    }
                    if (content == NULL)
                    {
                        content = NccLoadDiskHeader(incname, depth);
                    }
                    if (content == NULL)
                    {
                        char buf[96];
                        strncpy(buf, "cannot find include <", sizeof(buf) - 1U);
                        strncpy(buf + strlen(buf), incname,
                                sizeof(buf) - strlen(buf) - 1U);
                        strncpy(buf + strlen(buf), ">",
                                sizeof(buf) - strlen(buf) - 1U);
                        NccErrorAt(buf, line);
                        g_pp_ok = false;
                    }
                    else
                    {
                        NccBufStr(out, "\n# ");
                        NccBufStr(out, incname);
                        NccBufPut(out, '\n');
                        g_file = incname;
                        NccPpProcess(incname, content, out, depth + 1);
                        NccBufStr(out, "\n# ");
                        NccBufStr(out, fname);
                        NccBufPut(out, '\n');
                        g_file = fname;
                    }
                }
                else if (strcmp(w, "define") == 0)
                {
                    const char *s = rest;
                    while (s < start + len && NccIsWs(*s))
                    {
                        s++;
                    }
                    const char *ns = s;
                    while (s < start + len && NccIsIdentChar(*s))
                    {
                        s++;
                    }
                    uint32_t nlen = (uint32_t)(s - ns);
                    if (nlen == 0)
                    {
                        NccErrorAt("bad #define", line);
                        g_pp_ok = false;
                    }
                    else if (s < start + len && *s == '(')
                    {
                        NccErrorAt("function-like macros are not supported", line);
                        g_pp_ok = false;
                    }
                    else
                    {
                        while (s < start + len && NccIsWs(*s))
                        {
                            s++;
                        }
                        char name[64];
                        char repl[512];
                        if (nlen >= sizeof(name))
                        {
                            nlen = sizeof(name) - 1U;
                        }
                        memcpy(name, ns, nlen);
                        name[nlen] = '\0';
                        uint32_t rlen = (uint32_t)(start + len - s);
                        if (rlen >= sizeof(repl))
                        {
                            rlen = sizeof(repl) - 1U;
                        }
                        memcpy(repl, s, rlen);
                        repl[rlen] = '\0';
                        NccMacroDefine(name, repl);
                    }
                }
                else if (strcmp(w, "undef") == 0)
                {
                    const char *s = rest;
                    while (s < start + len && NccIsWs(*s))
                    {
                        s++;
                    }
                    char name[64];
                    uint32_t n = 0;
                    while (s < start + len && NccIsIdentChar(*s) &&
                           n < sizeof(name) - 1U)
                    {
                        name[n++] = *s++;
                    }
                    name[n] = '\0';
                    NccMacroUndef(name);
                }
                else if (strcmp(w, "pragma") != 0 && strcmp(w, "error") != 0)
                {
                    NccErrorAt("unknown preprocessor directive", line);
                    g_pp_ok = false;
                }
            }
        }
        else if (active)
        {
            NccEmitLineStripped(out, start, len, &in_comment);
        }
        if (*p == '\n')
        {
            p++;
        }
        NccBufPut(out, '\n');
        line++;
    }
}

/* ------------------------------------------------------------------ */
/* lexer                                                               */
/* ------------------------------------------------------------------ */

typedef enum
{
    TK_EOF = 0,
    TK_IDENT, TK_NUM, TK_STR, TK_CHR,
    TK_IF, TK_ELSE, TK_WHILE, TK_FOR, TK_DO, TK_RETURN, TK_BREAK, TK_CONTINUE,
    TK_TYPEDEF, TK_STRUCT, TK_ENUM, TK_SIZEOF,
    TK_CONST, TK_VOLATILE, TK_STATIC, TK_EXTERN, TK_INLINE, TK_ASM,
    TK_INT, TK_CHAR, TK_VOID, TK_LONG, TK_SHORT, TK_UNSIGNED, TK_SIGNED,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_EQEQ, TK_NEQ, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_SHL, TK_SHR, TK_ANDAND, TK_OROR, TK_BANG, TK_TILDE,
    TK_AMP, TK_PIPE, TK_CARET,
    TK_ASSIGN, TK_PLUSEQ, TK_MINUSEQ, TK_STAREQ, TK_SLASHEQ, TK_PERCENTEQ,
    TK_AMPEQ, TK_PIPEEQ, TK_CARETEQ, TK_SHLASSIGN, TK_SHRASSIGN,
    TK_INC, TK_DEC, TK_QUESTION, TK_COLON, TK_SEMI, TK_COMMA,
    TK_LPAREN, TK_RPAREN, TK_LBRACK, TK_RBRACK, TK_LBRACE, TK_RBRACE,
    TK_DOT, TK_ARROW,
    TK_UNKNOWN
} NccTokKind;

typedef struct
{
    int kind;
    uint64_t ival;
    char sval[80];
    uint32_t line;
    uint32_t str_len;
} NccTok;

static const char *g_lex_p;
static uint32_t g_lex_line;
static bool g_lex_line_start;
static uint8_t g_strbuf[NCC_MAX_STR];
static NccTok g_tok;
static NccTok g_peek;
static bool g_peek_valid;

static int NccStrncmp(const char *a, const char *b, uint64_t n)
{
    for (uint64_t i = 0; i < n; i++)
    {
        if (a[i] != b[i])
        {
            return (uint8_t)a[i] - (uint8_t)b[i];
        }
        if (a[i] == '\0')
        {
            return 0;
        }
    }
    return 0;
}

static void NccLexSkipWs(void)
{
    for (;;)
    {
        if (*g_lex_p == '\n')
        {
            g_lex_line++;
            g_lex_line_start = true;
            g_lex_p++;
        }
        else if (*g_lex_p == ' ' || *g_lex_p == '\t' || *g_lex_p == '\r')
        {
            g_lex_p++;
        }
        else if (*g_lex_p == '/' && g_lex_p[1] == '*')
        {
            g_lex_p += 2;
            while (*g_lex_p != '\0')
            {
                if (*g_lex_p == '\n')
                {
                    g_lex_line++;
                }
                if (*g_lex_p == '*' && g_lex_p[1] == '/')
                {
                    g_lex_p += 2;
                    break;
                }
                g_lex_p++;
            }
            g_lex_line_start = false;
        }
        else if (*g_lex_p == '/' && g_lex_p[1] == '/')
        {
            while (*g_lex_p != '\0' && *g_lex_p != '\n')
            {
                g_lex_p++;
            }
        }
        else if (*g_lex_p == '#' && g_lex_line_start)
        {
            g_lex_p++;
            const char *s = g_lex_p;
            while (*g_lex_p != '\0' && *g_lex_p != '\n')
            {
                g_lex_p++;
            }
            uint32_t n = (uint32_t)(g_lex_p - s);
            while (n > 0 && NccIsWs(s[n - 1U]))
            {
                n--;
            }
            if (n > 0)
            {
                static char filebuf[128];
                if (n >= sizeof(filebuf))
                {
                    n = sizeof(filebuf) - 1U;
                }
                memcpy(filebuf, s, n);
                filebuf[n] = '\0';
                g_file = filebuf;
            }
        }
        else
        {
            g_lex_line_start = false;
            return;
        }
    }
}

static int NccKw(const char *s)
{
    if (strcmp(s, "if") == 0) return TK_IF;
    if (strcmp(s, "else") == 0) return TK_ELSE;
    if (strcmp(s, "while") == 0) return TK_WHILE;
    if (strcmp(s, "for") == 0) return TK_FOR;
    if (strcmp(s, "do") == 0) return TK_DO;
    if (strcmp(s, "return") == 0) return TK_RETURN;
    if (strcmp(s, "break") == 0) return TK_BREAK;
    if (strcmp(s, "continue") == 0) return TK_CONTINUE;
    if (strcmp(s, "typedef") == 0) return TK_TYPEDEF;
    if (strcmp(s, "struct") == 0) return TK_STRUCT;
    if (strcmp(s, "enum") == 0) return TK_ENUM;
    if (strcmp(s, "sizeof") == 0) return TK_SIZEOF;
    if (strcmp(s, "const") == 0) return TK_CONST;
    if (strcmp(s, "volatile") == 0) return TK_VOLATILE;
    if (strcmp(s, "static") == 0) return TK_STATIC;
    if (strcmp(s, "extern") == 0) return TK_EXTERN;
    if (strcmp(s, "inline") == 0) return TK_INLINE;
    if (strcmp(s, "asm") == 0) return TK_ASM;
    if (strcmp(s, "int") == 0) return TK_INT;
    if (strcmp(s, "char") == 0) return TK_CHAR;
    if (strcmp(s, "void") == 0) return TK_VOID;
    if (strcmp(s, "long") == 0) return TK_LONG;
    if (strcmp(s, "short") == 0) return TK_SHORT;
    if (strcmp(s, "unsigned") == 0) return TK_UNSIGNED;
    if (strcmp(s, "signed") == 0) return TK_SIGNED;
    return TK_IDENT;
}

static void NccLexNumber(NccTok *t)
{
    const char *p = g_lex_p;
    uint64_t v = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        p += 2;
        while (NccIsHex(*p))
        {
            v = (v << 4) | NccHexVal(*p);
            p++;
        }
    }
    else if (p[0] == '0' && p[1] >= '0' && p[1] <= '7')
    {
        p++;
        while (*p >= '0' && *p <= '7')
        {
            v = (v << 3) | (uint64_t)(*p - '0');
            p++;
        }
    }
    else
    {
        while (*p >= '0' && *p <= '9')
        {
            v = v * 10 + (uint64_t)(*p - '0');
            p++;
        }
    }
    bool is_unsigned = false;
    bool is_long = false;
    while (*p == 'u' || *p == 'U')
    {
        is_unsigned = true;
        p++;
    }
    while (*p == 'l' || *p == 'L')
    {
        is_long = true;
        p++;
    }
    g_lex_p = p;
    t->kind = TK_NUM;
    t->ival = v;
    t->sval[0] = '\0';
    if (is_unsigned)
    {
        t->sval[0] = 'u';
    }
    if (is_long)
    {
        t->sval[0] = 'l';
    }
    t->sval[1] = '\0';
}

static int NccCharEscape(const char **pp)
{
    char c = **pp;
    (*pp)++;
    switch (c)
    {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '0': return '\0';
        case 'a': return 7;
        case 'b': return 8;
        case 'f': return 12;
        case 'v': return 11;
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case 'x':
        {
            int v = 0;
            int n = 0;
            while (NccIsHex(**pp) && n < 2)
            {
                v = (v << 4) | (int)NccHexVal(**pp);
                (*pp)++;
                n++;
            }
            return v;
        }
        default:
            return (int)(uint8_t)c;
    }
}

static void NccLexString(uint32_t *out_len)
{
    const char *p = g_lex_p;
    uint32_t n = 0;
    while (*p != '\0' && *p != '"' && n < NCC_MAX_STR)
    {
        if (*p == '\\')
        {
            p++;
            if (*p == '\0')
            {
                break;
            }
            g_strbuf[n++] = (uint8_t)NccCharEscape(&p);
        }
        else
        {
            g_strbuf[n++] = (uint8_t)*p++;
        }
    }
    if (*p == '"')
    {
        p++;
    }
    g_lex_p = p;
    *out_len = n;
}

static void NccLexNext(NccTok *t)
{
    NccLexSkipWs();
    t->line = g_lex_line;
    t->str_len = 0;
    char c = *g_lex_p;
    if (c == '\0')
    {
        t->kind = TK_EOF;
        return;
    }
    if (NccIsIdentStart(c))
    {
        char name[80];
        uint32_t n = 0;
        while (NccIsIdentChar(*g_lex_p) && n < sizeof(name) - 1U)
        {
            name[n++] = *g_lex_p++;
        }
        name[n] = '\0';
        if (strcmp(name, "__attribute__") == 0 ||
            strcmp(name, "__attribute") == 0)
        {
            if (*g_lex_p == '(')
            {
                uint32_t depth = 0;
                while (*g_lex_p != '\0')
                {
                    if (*g_lex_p == '(')
                    {
                        depth++;
                    }
                    else if (*g_lex_p == ')')
                    {
                        if (depth == 0)
                        {
                            break;
                        }
                        depth--;
                    }
                    else if (*g_lex_p == '\n')
                    {
                        g_lex_line++;
                    }
                    g_lex_p++;
                    if (depth == 0)
                    {
                        break;
                    }
                }
            }
            NccLexNext(t);
            return;
        }
        int kw = NccKw(name);
        if (kw == TK_IDENT)
        {
            t->kind = TK_IDENT;
            strncpy(t->sval, name, sizeof(t->sval) - 1U);
        }
        else
        {
            t->kind = kw;
        }
        return;
    }
    if (c >= '0' && c <= '9')
    {
        NccLexNumber(t);
        return;
    }
    if (c == '"')
    {
        g_lex_p++;
        t->kind = TK_STR;
        NccLexString(&t->str_len);
        return;
    }
    if (c == '\'')
    {
        g_lex_p++;
        t->kind = TK_CHR;
        if (*g_lex_p == '\\')
        {
            g_lex_p++;
            t->ival = (uint64_t)NccCharEscape(&g_lex_p);
        }
        else if (*g_lex_p != '\'')
        {
            t->ival = (uint8_t)*g_lex_p++;
        }
        else
        {
            t->ival = 0;
        }
        if (*g_lex_p == '\'')
        {
            g_lex_p++;
        }
        return;
    }
    g_lex_p++;
    switch (c)
    {
        case '+':
            if (*g_lex_p == '+') { g_lex_p++; t->kind = TK_INC; }
            else if (*g_lex_p == '=') { g_lex_p++; t->kind = TK_PLUSEQ; }
            else t->kind = TK_PLUS;
            return;
        case '-':
            if (*g_lex_p == '-') { g_lex_p++; t->kind = TK_DEC; }
            else if (*g_lex_p == '=') { g_lex_p++; t->kind = TK_MINUSEQ; }
            else if (*g_lex_p == '>') { g_lex_p++; t->kind = TK_ARROW; }
            else t->kind = TK_MINUS;
            return;
        case '*': t->kind = (*g_lex_p == '=') ? (g_lex_p++, TK_STAREQ) : TK_STAR; return;
        case '/': t->kind = (*g_lex_p == '=') ? (g_lex_p++, TK_SLASHEQ) : TK_SLASH; return;
        case '%': t->kind = (*g_lex_p == '=') ? (g_lex_p++, TK_PERCENTEQ) : TK_PERCENT; return;
        case '<':
            if (*g_lex_p == '<')
            {
                g_lex_p++;
                if (*g_lex_p == '=') { g_lex_p++; t->kind = TK_SHLASSIGN; }
                else t->kind = TK_SHL;
            }
            else if (*g_lex_p == '=') { g_lex_p++; t->kind = TK_LE; }
            else t->kind = TK_LT;
            return;
        case '>':
            if (*g_lex_p == '>')
            {
                g_lex_p++;
                if (*g_lex_p == '=') { g_lex_p++; t->kind = TK_SHRASSIGN; }
                else t->kind = TK_SHR;
            }
            else if (*g_lex_p == '=') { g_lex_p++; t->kind = TK_GE; }
            else t->kind = TK_GT;
            return;
        case '=': t->kind = (*g_lex_p == '=') ? (g_lex_p++, TK_EQEQ) : TK_ASSIGN; return;
        case '!': t->kind = (*g_lex_p == '=') ? (g_lex_p++, TK_NEQ) : TK_BANG; return;
        case '&':
            if (*g_lex_p == '&') { g_lex_p++; t->kind = TK_ANDAND; }
            else if (*g_lex_p == '=') { g_lex_p++; t->kind = TK_AMPEQ; }
            else t->kind = TK_AMP;
            return;
        case '|':
            if (*g_lex_p == '|') { g_lex_p++; t->kind = TK_OROR; }
            else if (*g_lex_p == '=') { g_lex_p++; t->kind = TK_PIPEEQ; }
            else t->kind = TK_PIPE;
            return;
        case '^': t->kind = (*g_lex_p == '=') ? (g_lex_p++, TK_CARETEQ) : TK_CARET; return;
        case '~': t->kind = TK_TILDE; return;
        case '?': t->kind = TK_QUESTION; return;
        case ':': t->kind = TK_COLON; return;
        case ';': t->kind = TK_SEMI; return;
        case ',': t->kind = TK_COMMA; return;
        case '(': t->kind = TK_LPAREN; return;
        case ')': t->kind = TK_RPAREN; return;
        case '[': t->kind = TK_LBRACK; return;
        case ']': t->kind = TK_RBRACK; return;
        case '{': t->kind = TK_LBRACE; return;
        case '}': t->kind = TK_RBRACE; return;
        case '.': t->kind = TK_DOT; return;
        default: t->kind = TK_UNKNOWN; return;
    }
}

static void NccAdvance(void)
{
    if (!g_peek_valid)
    {
        NccLexNext(&g_peek);
        g_peek_valid = true;
    }
    g_tok = g_peek;
    NccLexNext(&g_peek);
}

/* ------------------------------------------------------------------ */
/* type model                                                          */
/* ------------------------------------------------------------------ */

static NccType *NccPtrType(NccType *elem)
{
    NccType *t = (NccType *)kmalloc(sizeof(NccType));
    t->kind = TY_PTR;
    t->size = 8;
    t->is_unsigned = true;
    t->elem = elem;
    t->arr_len = 0;
    t->name[0] = '\0';
    return t;
}

static NccType *NccArrType(NccType *elem, int len)
{
    NccType *t = (NccType *)kmalloc(sizeof(NccType));
    t->kind = TY_ARR;
    t->size = elem->size * len;
    t->is_unsigned = false;
    t->elem = elem;
    t->arr_len = len;
    t->name[0] = '\0';
    return t;
}

static NccType *NccOpqType(const char *name)
{
    NccType *t = (NccType *)kmalloc(sizeof(NccType));
    t->kind = TY_OPQ;
    t->size = 8;
    t->is_unsigned = true;
    t->elem = NULL;
    t->arr_len = 0;
    strncpy(t->name, name, sizeof(t->name) - 1U);
    t->name[sizeof(t->name) - 1U] = '\0';
    return t;
}

static bool NccTypeIs64(NccType *t)
{
    return t != NULL && t->size == 8;
}

static bool NccTypeIsUnsigned(NccType *t)
{
    return t != NULL && (t->is_unsigned || t->kind == TY_PTR);
}

static NccType *NccTypeDecay(NccType *t)
{
    if (t != NULL && t->kind == TY_ARR)
    {
        return t->elem;
    }
    return t;
}

static int NccTypeAlign(NccType *t)
{
    if (t->size >= 8)
    {
        return 8;
    }
    if (t->size >= 4)
    {
        return 4;
    }
    if (t->size >= 2)
    {
        return 2;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* symbol tables                                                       */
/* ------------------------------------------------------------------ */

typedef struct
{
    char name[64];
    NccType *type;
    int kind;
    int offset;
    uint64_t addr;
} NccVar;

typedef struct
{
    char name[64];
    NccType *ret;
    NccType *params[NCC_MAX_PARAMS];
    uint32_t nparams;
    bool defined;
    bool has_proto;
    uint32_t text_rel;
} NccFunc;

static NccVar g_globals[NCC_MAX_GLOBALS];
static uint32_t g_globals_count;
static NccVar g_locals[NCC_MAX_LOCALS];
static uint32_t g_locals_count;
static uint32_t g_scope_stack[NCC_MAX_SCOPES];
static uint32_t g_scope_depth;
static NccFunc g_funcs[NCC_MAX_FUNCS];
static uint32_t g_func_count;
static NccType *g_typedefs[NCC_MAX_TYPEDEFS];
static int g_local_off;
static int g_local_min_off;
static char g_typedef_names[NCC_MAX_TYPEDEFS][64];
static uint32_t g_typedef_count;

static int NccFindFunc(const char *name)
{
    for (uint32_t i = 0; i < g_func_count; i++)
    {
        if (strcmp(g_funcs[i].name, name) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

static int NccFindGlobal(const char *name)
{
    for (uint32_t i = 0; i < g_globals_count; i++)
    {
        if (strcmp(g_globals[i].name, name) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

static NccVar *NccFindLocal(const char *name)
{
    for (uint32_t i = g_locals_count; i > 0; i--)
    {
        if (strcmp(g_locals[i - 1U].name, name) == 0)
        {
            return &g_locals[i - 1U];
        }
    }
    return NULL;
}

static NccType *NccFindTypedef(const char *name)
{
    for (uint32_t i = 0; i < g_typedef_count; i++)
    {
        if (strcmp(g_typedef_names[i], name) == 0)
        {
            return g_typedefs[i];
        }
    }
    return NULL;
}

static void NccAddTypedef(const char *name, NccType *type)
{
    if (g_typedef_count >= NCC_MAX_TYPEDEFS)
    {
        return;
    }
    if (NccFindTypedef(name) != NULL)
    {
        return;
    }
    strncpy(g_typedef_names[g_typedef_count], name, 63U);
    g_typedef_names[g_typedef_count][63] = '\0';
    g_typedefs[g_typedef_count] = type;
    g_typedef_count++;
}

static void NccScopePush(void)
{
    if (g_scope_depth < NCC_MAX_SCOPES)
    {
        g_scope_stack[g_scope_depth++] = g_locals_count;
    }
}

static void NccScopePop(void)
{
    if (g_scope_depth > 0)
    {
        g_scope_depth--;
        g_locals_count = g_scope_stack[g_scope_depth];
    }
}

/* ------------------------------------------------------------------ */
/* code buffers + fixups                                               */
/* ------------------------------------------------------------------ */

static uint8_t *g_text;
static uint32_t g_text_size;
static uint32_t g_text_cap;
static uint8_t *g_data;
static uint32_t g_data_size;
static uint32_t g_data_cap;
static uint32_t g_next_label;
static uint32_t g_estack;
static int g_loop_break[NCC_MAX_LOOP];
static int g_loop_cont[NCC_MAX_LOOP];
static uint32_t g_loop_depth;
static uint32_t g_str_counter;

typedef struct
{
    uint32_t pos;
    char name[80];
} NccRef;

typedef struct
{
    uint32_t pos;
    uint32_t label;
} NccJump;

typedef struct
{
    uint32_t label;
    uint32_t pos;
} NccLDef;

static NccRef g_refs[NCC_MAX_FIXUPS];
static uint32_t g_ref_count;
static NccJump g_jumps[NCC_MAX_FIXUPS];
static uint32_t g_jump_count;
static NccLDef g_labels[NCC_MAX_FIXUPS];
static uint32_t g_label_count;
static char g_str_name[16];

static void NccTextReserve(uint32_t n)
{
    if (g_text_size + n > g_text_cap)
    {
        uint32_t nc = g_text_cap == 0U ? 4096U : g_text_cap * 2U;
        while (nc < g_text_size + n)
        {
            nc *= 2U;
        }
        uint8_t *nt = (uint8_t *)kmalloc(nc);
        if (g_text_size > 0)
        {
            memcpy(nt, g_text, g_text_size);
        }
        g_text = nt;
        g_text_cap = nc;
    }
}

static void NccTextByte(uint8_t b)
{
    NccTextReserve(1);
    g_text[g_text_size++] = b;
}

static void NccTextU32(uint32_t v)
{
    NccTextReserve(4);
    g_text[g_text_size++] = (uint8_t)(v & 0xFFU);
    g_text[g_text_size++] = (uint8_t)((v >> 8) & 0xFFU);
    g_text[g_text_size++] = (uint8_t)((v >> 16) & 0xFFU);
    g_text[g_text_size++] = (uint8_t)((v >> 24) & 0xFFU);
}

static void NccTextU64(uint64_t v)
{
    for (uint32_t i = 0; i < 8; i++)
    {
        NccTextByte((uint8_t)((v >> (i * 8U)) & 0xFFU));
    }
}

static void NccTextBytes(const void *src, uint32_t n)
{
    NccTextReserve(n);
    memcpy(g_text + g_text_size, src, n);
    g_text_size += n;
}

static uint32_t NccNewLabel(void)
{
    return g_next_label++;
}

static void NccDefLabel(uint32_t label)
{
    if (g_label_count < NCC_MAX_FIXUPS)
    {
        g_labels[g_label_count].label = label;
        g_labels[g_label_count].pos = g_text_size;
        g_label_count++;
    }
}

static void NccJmpRel(int opcode, uint32_t label)
{
    NccTextByte((uint8_t)opcode);
    if (g_jump_count < NCC_MAX_FIXUPS)
    {
        g_jumps[g_jump_count].pos = g_text_size;
        g_jumps[g_jump_count].label = label;
        g_jump_count++;
    }
    NccTextU32(0);
}

static void NccJcc(uint8_t cc, uint32_t label)
{
    NccTextByte(0x0F);
    NccTextByte(cc);
    if (g_jump_count < NCC_MAX_FIXUPS)
    {
        g_jumps[g_jump_count].pos = g_text_size;
        g_jumps[g_jump_count].label = label;
        g_jump_count++;
    }
    NccTextU32(0);
}

static void NccJz(uint32_t label) { NccJcc(0x84, label); }
static void NccJnz(uint32_t label) { NccJcc(0x85, label); }

static void NccJmp(uint32_t label)
{
    NccTextByte(0xE9);
    if (g_jump_count < NCC_MAX_FIXUPS)
    {
        g_jumps[g_jump_count].pos = g_text_size;
        g_jumps[g_jump_count].label = label;
        g_jump_count++;
    }
    NccTextU32(0);
}

static void NccRefAbs(const char *name)
{
    NccTextByte(0x48);
    NccTextByte(0xB8);
    if (g_ref_count < NCC_MAX_FIXUPS)
    {
        g_refs[g_ref_count].pos = g_text_size;
        strncpy(g_refs[g_ref_count].name, name, 79U);
        g_refs[g_ref_count].name[79] = '\0';
        g_ref_count++;
    }
    NccTextU64(0);
}

static void NccEmitPushRax(void)
{
    NccTextByte(0x50);
    g_estack++;
}

static void NccEmitPushRcx(void)
{
    NccTextByte(0x51);
    g_estack++;
}

static void NccEmitPop(int reg)
{
    if (reg < 8)
    {
        NccTextByte((uint8_t)(0x58 + reg));
    }
    else
    {
        NccTextByte(0x41);
        NccTextByte((uint8_t)(0x58 + reg - 8));
    }
    g_estack--;
}

static void NccEmitSwap(void)
{
    NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0xC2);
    NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0xC8);
    NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0xD1);
}

static void NccEmitCallRef(const char *name)
{
    NccRefAbs(name);
    NccTextByte(0x49);
    NccTextByte(0x89);
    NccTextByte(0xC3);
    NccTextByte(0x31);
    NccTextByte(0xC0);
    NccTextByte(0x41);
    NccTextByte(0xFF);
    NccTextByte(0xD3);
}

static void NccEmitTestRax(void)
{
    NccTextByte(0x48);
    NccTextByte(0x85);
    NccTextByte(0xC0);
}

static void NccEmitMovEaxImm(uint32_t v)
{
    NccTextByte(0xB8);
    NccTextU32(v);
}

static void NccEmitMovAbsImm(uint64_t v)
{
    NccTextByte(0x48);
    NccTextByte(0xB8);
    NccTextU64(v);
}

static void NccEmitSetCC(uint8_t cc)
{
    NccTextByte(0x0F);
    NccTextByte(cc);
    NccTextByte(0xC0);
    NccTextByte(0x0F);
    NccTextByte(0xB6);
    NccTextByte(0xC0);
}

static void NccEmitTrunc(NccType *t)
{
    if (t == NULL || t->size == 0)
    {
        return;
    }
    if (t->size == 4)
    {
        NccTextByte(0x89); NccTextByte(0xC0);
        if (!t->is_unsigned)
        {
            NccTextByte(0x48); NccTextByte(0x63); NccTextByte(0xC0);
        }
    }
    else if (t->size == 1)
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB6 : 0xBE);
        NccTextByte(0xC0);
    }
    else if (t->size == 2)
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB7 : 0xBF);
        NccTextByte(0xC0);
    }
}

static void NccEmitBinop(int op, bool w64)
{
    if (w64)
    {
        NccTextByte(0x48);
    }
    switch (op)
    {
        case TK_PLUS: NccTextByte(0x01); NccTextByte(0xC8); break;
        case TK_MINUS: NccTextByte(0x29); NccTextByte(0xC8); break;
        case TK_AMP: NccTextByte(0x21); NccTextByte(0xC8); break;
        case TK_PIPE: NccTextByte(0x09); NccTextByte(0xC8); break;
        case TK_CARET: NccTextByte(0x31); NccTextByte(0xC8); break;
        case TK_STAR:
            NccTextByte(0x0F); NccTextByte(0xAF); NccTextByte(0xC1);
            break;
        case TK_SHL:
            if (w64)
            {
                NccTextByte(0x48);
            }
            NccTextByte(0xD3); NccTextByte(0xE0);
            break;
        case TK_SHR:
            if (w64)
            {
                NccTextByte(0x48);
            }
            NccTextByte(0xD3); NccTextByte(0xE8);
            break;
        default:
            break;
    }
}

static void NccEmitDiv(bool is_mod, bool w64, bool is_unsigned)
{
    if (!w64)
    {
        NccTextByte(0x89); NccTextByte(0xC0);
        if (is_unsigned)
        {
            NccTextByte(0x31); NccTextByte(0xD2);
            NccTextByte(0xF7); NccTextByte(0xF1);
            if (is_mod)
            {
                NccTextByte(0x89); NccTextByte(0xD0);
            }
        }
        else
        {
            NccTextByte(0x99);
            NccTextByte(0xF7); NccTextByte(0xF9);
            if (is_mod)
            {
                NccTextByte(0x89); NccTextByte(0xD0);
                NccTextByte(0x48); NccTextByte(0x63); NccTextByte(0xC0);
            }
            else
            {
                NccTextByte(0x48); NccTextByte(0x63); NccTextByte(0xC0);
            }
        }
    }
    else
    {
        if (is_unsigned)
        {
            NccTextByte(0x31); NccTextByte(0xD2);
            NccTextByte(0x48); NccTextByte(0xF7); NccTextByte(0xF1);
        }
        else
        {
            NccTextByte(0x48); NccTextByte(0x99);
            NccTextByte(0x48); NccTextByte(0xF7); NccTextByte(0xF9);
        }
        if (is_mod)
        {
            NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0xD0);
        }
    }
}

static void NccEmitCmp(int op, bool is_unsigned)
{
    NccTextByte(0x48); NccTextByte(0x39); NccTextByte(0xC8);
    switch (op)
    {
        case TK_EQEQ: NccEmitSetCC(0x94); break;
        case TK_NEQ: NccEmitSetCC(0x95); break;
        case TK_LT: NccEmitSetCC(is_unsigned ? 0x92 : 0x9C); break;
        case TK_LE: NccEmitSetCC(is_unsigned ? 0x96 : 0x9E); break;
        case TK_GT: NccEmitSetCC(is_unsigned ? 0x97 : 0x9F); break;
        case TK_GE: NccEmitSetCC(is_unsigned ? 0x93 : 0x9D); break;
        default: break;
    }
}

static void NccEmitNeg(bool w64)
{
    if (w64)
    {
        NccTextByte(0x48);
    }
    NccTextByte(0xF7);
    NccTextByte(0xD8);
}

static void NccEmitNot(bool w64)
{
    if (w64)
    {
        NccTextByte(0x48);
    }
    NccTextByte(0xF7);
    NccTextByte(0xD0);
}

static void NccEmitIncRax(void)
{
    NccTextByte(0x48); NccTextByte(0xFF); NccTextByte(0xC0);
}

static void NccEmitDecRax(void)
{
    NccTextByte(0x48); NccTextByte(0xFF); NccTextByte(0xC8);
}

static void NccEmitAddRaxRcx(void)
{
    NccTextByte(0x48); NccTextByte(0x01); NccTextByte(0xC8);
}

static void NccEmitShlImm(uint8_t count)
{
    if (count > 0)
    {
        NccTextByte(0x48); NccTextByte(0xC1); NccTextByte(0xE0);
        NccTextByte(count);
    }
}

static void NccEmitShrImm(uint8_t count)
{
    if (count > 0)
    {
        NccTextByte(0x48); NccTextByte(0xC1); NccTextByte(0xE8);
        NccTextByte(count);
    }
}

static void NccEmitSubRsp8(void)
{
    NccTextByte(0x48); NccTextByte(0x83); NccTextByte(0xEC); NccTextByte(8);
}

static void NccEmitAddRsp8(void)
{
    NccTextByte(0x48); NccTextByte(0x83); NccTextByte(0xC4); NccTextByte(8);
}

static void NccEmitMovRcxFromRcxPtr(NccType *t)
{
    if (t->size == 8)
    {
        NccTextByte(0x48); NccTextByte(0x8B); NccTextByte(0x09);
    }
    else if (t->size == 4)
    {
        NccTextByte(0x8B); NccTextByte(0x09);
        if (!t->is_unsigned)
        {
            NccTextByte(0x48); NccTextByte(0x63); NccTextByte(0x09);
        }
    }
    else if (t->size == 2)
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB7 : 0xBF);
        NccTextByte(0x09);
    }
    else
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB6 : 0xBE);
        NccTextByte(0x09);
    }
}

static void NccEmitLoadRaxFromRaxPtr(NccType *t)
{
    if (t->size == 8)
    {
        NccTextByte(0x48); NccTextByte(0x8B); NccTextByte(0x00);
    }
    else if (t->size == 4)
    {
        NccTextByte(0x8B); NccTextByte(0x00);
        if (!t->is_unsigned)
        {
            NccTextByte(0x48); NccTextByte(0x63); NccTextByte(0xC0);
        }
    }
    else if (t->size == 2)
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB7 : 0xBF);
        NccTextByte(0x00);
    }
    else
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB6 : 0xBE);
        NccTextByte(0x00);
    }
}

static void NccEmitStoreRaxToRcxPtr(NccType *t)
{
    if (t->size == 8)
    {
        NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0x01);
    }
    else if (t->size == 4)
    {
        NccTextByte(0x89); NccTextByte(0x01);
    }
    else if (t->size == 2)
    {
        NccTextByte(0x66); NccTextByte(0x89); NccTextByte(0x01);
    }
    else
    {
        NccTextByte(0x88); NccTextByte(0x01);
    }
}

static void NccEmitLocalLoad(NccType *t, int off)
{
    uint32_t disp = (uint32_t)off;
    if (t->size == 8)
    {
        NccTextByte(0x48); NccTextByte(0x8B); NccTextByte(0x85);
        NccTextU32(disp);
    }
    else if (t->size == 4)
    {
        NccTextByte(0x8B); NccTextByte(0x85);
        NccTextU32(disp);
        if (!t->is_unsigned)
        {
            NccTextByte(0x48); NccTextByte(0x63); NccTextByte(0x85);
            NccTextU32(disp);
        }
    }
    else if (t->size == 2)
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB7 : 0xBF);
        NccTextByte(0x85);
        NccTextU32(disp);
    }
    else
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB6 : 0xBE);
        NccTextByte(0x85);
        NccTextU32(disp);
    }
}

static void NccEmitLocalStore(NccType *t, int off)
{
    uint32_t disp = (uint32_t)off;
    if (t->size == 8)
    {
        NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0x85);
        NccTextU32(disp);
    }
    else if (t->size == 4)
    {
        NccTextByte(0x89); NccTextByte(0x85);
        NccTextU32(disp);
    }
    else if (t->size == 2)
    {
        NccTextByte(0x66); NccTextByte(0x89); NccTextByte(0x85);
        NccTextU32(disp);
    }
    else
    {
        NccTextByte(0x88); NccTextByte(0x85);
        NccTextU32(disp);
    }
}

static void NccEmitLeaLocal(int off)
{
    NccTextByte(0x48); NccTextByte(0x8D); NccTextByte(0x85);
    NccTextU32((uint32_t)off);
}

static void NccEmitArgStore(int reg, NccType *t, int off)
{
    uint32_t disp = (uint32_t)off;
    int enc = reg & 7;
    bool rex = reg >= 8;
    uint8_t modrm = (uint8_t)(0x85 | (enc << 3));
    if (t->size == 8)
    {
        NccTextByte(rex ? 0x49 : 0x48);
        NccTextByte(0x89);
        NccTextByte(modrm);
        NccTextU32(disp);
    }
    else if (t->size == 4)
    {
        if (rex)
        {
            NccTextByte(0x44);
        }
        NccTextByte(0x89);
        NccTextByte(modrm);
        NccTextU32(disp);
    }
    else if (t->size == 2)
    {
        NccTextByte(0x66);
        if (rex)
        {
            NccTextByte(0x44);
        }
        NccTextByte(0x89);
        NccTextByte(modrm);
        NccTextU32(disp);
    }
    else
    {
        if (rex)
        {
            NccTextByte(0x44);
        }
        else if (reg == 6 || reg == 7)
        {
            NccTextByte(0x40);
        }
        NccTextByte(0x88);
        NccTextByte(modrm);
        NccTextU32(disp);
    }
}

static void NccEmitPrologueTo(uint8_t *pro, uint32_t *plen, uint32_t frame)
{
    uint32_t n = 0;
    pro[n++] = 0x55;
    pro[n++] = 0x48; pro[n++] = 0x89; pro[n++] = 0xE5;
    if (frame > 0)
    {
        pro[n++] = 0x48; pro[n++] = 0x81; pro[n++] = 0xEC;
        pro[n++] = (uint8_t)(frame & 0xFFU);
        pro[n++] = (uint8_t)((frame >> 8) & 0xFFU);
        pro[n++] = (uint8_t)((frame >> 16) & 0xFFU);
        pro[n++] = (uint8_t)((frame >> 24) & 0xFFU);
    }
    *plen = n;
}

static void NccEmitEpilogue(void)
{
    NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0xEC);
    NccTextByte(0x5D);
    NccTextByte(0xC3);
}

static uint64_t NccDataAlloc(uint32_t size, uint32_t align)
{
    uint32_t pad = (align - (g_data_size % align)) % align;
    g_data_size += pad;
    uint64_t vaddr = NCC_BASE + NCC_HDRSZ + g_data_size;
    while (g_data_cap < g_data_size + size)
    {
        uint32_t nc = g_data_cap == 0U ? 256U : g_data_cap * 2U;
        uint8_t *nd = (uint8_t *)kmalloc(nc);
        if (g_data_size > 0)
        {
            memcpy(nd, g_data, g_data_size);
        }
        g_data = nd;
        g_data_cap = nc;
    }
    memset(g_data + g_data_size, 0, size);
    g_data_size += size;
    return vaddr;
}

static void NccDataWrite(uint64_t vaddr, const void *src, uint32_t n)
{
    memcpy(g_data + (uint32_t)(vaddr - NCC_BASE - NCC_HDRSZ), src, n);
}

static uint64_t NccMakeString(const uint8_t *bytes, uint32_t len)
{
    char name[16];
    uint64_t vaddr;
    name[0] = '$';
    NccItoa(g_str_counter, name + 1);
    strncpy(g_str_name, name, sizeof(g_str_name) - 1U);
    g_str_name[sizeof(g_str_name) - 1U] = '\0';
    if (g_globals_count < NCC_MAX_GLOBALS)
    {
        NccVar *v = &g_globals[g_globals_count++];
        strncpy(v->name, name, sizeof(v->name) - 1U);
        v->name[sizeof(v->name) - 1U] = '\0';
        v->type = NccArrType(&g_ty_char, (int)len + 1);
        v->kind = VAR_GLOBAL;
        v->offset = 0;
        v->addr = NccDataAlloc(len + 1U, 1);
        NccDataWrite(v->addr, bytes, len);
        vaddr = v->addr;
    }
    else
    {
        vaddr = 0;
    }
    g_str_counter++;
    return vaddr;
}

/* ------------------------------------------------------------------ */
/* parser + codegen state                                              */
/* ------------------------------------------------------------------ */

static int g_frame_patch_pos;
static int g_local_save_off;
static int g_local_save_min;
static NccFunc *g_cur_func;
static NccType *g_last_type;
static uint32_t g_first_func_rel = UINT32_MAX;

typedef struct
{
    char name[64];
    uint64_t value;
} NccConst;

static NccConst g_consts[NCC_MAX_CONSTS];
static uint32_t g_const_count;

static void NccResetParser(void)
{
    g_peek_valid = false;
    g_last_type = &g_ty_int;
    g_first_func_rel = UINT32_MAX;
    g_frame_patch_pos = 0;
    g_cur_func = NULL;
    g_const_count = 0;
    g_loop_depth = 0;
    g_str_counter = 0;
    g_estack = 0;
    g_text_size = 0;
    g_data_size = 0;
    g_next_label = 0;
    g_ref_count = 0;
    g_jump_count = 0;
    g_label_count = 0;
    g_globals_count = 0;
    g_func_count = 0;
    g_locals_count = 0;
    g_scope_depth = 0;
    g_typedef_count = 0;
    g_local_off = 0;
    g_local_min_off = 0;
}

/* ------------------------------------------------------------------ */
/* extra codegen helpers                                               */
/* ------------------------------------------------------------------ */

static uint64_t NccConstExpr(void);

static bool g_last_expr_is_addr;
static bool g_last_is_func;
static char g_call_name[80];

static void NccEmitMovRaxToRcx(void)
{
    NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0xC1);
}

static void NccEmitMovRcxToRax(void)
{
    NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0xC8);
}

static void NccEmitMovRcxImm64(uint64_t v)
{
    NccTextByte(0x48); NccTextByte(0xB9);
    NccTextU64(v);
}

static void NccEmitXorRax(void)
{
    NccTextByte(0x48); NccTextByte(0x31); NccTextByte(0xC0);
}

static void NccEmitLoadRaxFromRcxPtr(NccType *t)
{
    if (t == NULL)
    {
        t = &g_ty_int;
    }
    if (t->size == 8)
    {
        NccTextByte(0x48); NccTextByte(0x8B); NccTextByte(0x01);
    }
    else if (t->size == 4)
    {
        NccTextByte(0x8B); NccTextByte(0x01);
        if (!t->is_unsigned)
        {
            NccTextByte(0x48); NccTextByte(0x63); NccTextByte(0x01);
        }
    }
    else if (t->size == 2)
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB7 : 0xBF);
        NccTextByte(0x01);
    }
    else
    {
        NccTextByte(0x48); NccTextByte(0x0F);
        NccTextByte(t->is_unsigned ? 0xB6 : 0xBE);
        NccTextByte(0x01);
    }
}

static void NccEmitImulRcxImm32(uint32_t v)
{
    NccTextByte(0x48); NccTextByte(0x69); NccTextByte(0xC9);
    NccTextU32(v);
}

static void NccEmitImulRaxImm32(uint32_t v)
{
    NccTextByte(0x48); NccTextByte(0x69); NccTextByte(0xC0);
    NccTextU32(v);
}

static void NccEmitMovRaxImm(uint64_t v, bool w64)
{
    if (!w64)
    {
        NccTextByte(0xB8);
        NccTextU32((uint32_t)v);
    }
    else if (v <= 0x7FFFFFFFULL || v >= 0xFFFFFFFF80000000ULL)
    {
        NccTextByte(0x48); NccTextByte(0xC7); NccTextByte(0xC0);
        NccTextU32((uint32_t)v);
    }
    else
    {
        NccTextByte(0x48); NccTextByte(0xB8);
        NccTextU64(v);
    }
}

static void NccRefAbsToReg(int reg, const char *name)
{
    if (reg < 8)
    {
        NccTextByte(0x48);
        NccTextByte((uint8_t)(0xB8 + reg));
    }
    else
    {
        NccTextByte(0x49);
        NccTextByte((uint8_t)(0xB8 + (reg - 8)));
    }
    if (g_ref_count < NCC_MAX_FIXUPS)
    {
        g_refs[g_ref_count].pos = g_text_size;
        strncpy(g_refs[g_ref_count].name, name, 79U);
        g_refs[g_ref_count].name[79] = '\0';
        g_ref_count++;
    }
    NccTextU64(0);
}

static void NccEmitLeaToReg(int reg, int off)
{
    uint8_t modrm = (uint8_t)(0x85 | ((reg & 7) << 3));
    if (reg >= 8)
    {
        NccTextByte(0x49);
    }
    else
    {
        NccTextByte(0x48);
    }
    NccTextByte(0x8D);
    NccTextByte(modrm);
    NccTextU32((uint32_t)off);
}

static void NccPatchU32(uint32_t pos, uint32_t v)
{
    memcpy(g_text + pos, &v, 4);
}

static void NccPatchU64(uint32_t pos, uint64_t v)
{
    memcpy(g_text + pos, &v, 8);
}

/* ------------------------------------------------------------------ */
/* type / declaration parsing                                          */
/* ------------------------------------------------------------------ */

static bool NccTokIsTypeStart(int kind)
{
    switch (kind)
    {
        case TK_VOID: case TK_CHAR: case TK_INT: case TK_LONG: case TK_SHORT:
        case TK_UNSIGNED: case TK_SIGNED: case TK_STRUCT: case TK_ENUM:
        case TK_CONST: case TK_VOLATILE: case TK_STATIC: case TK_EXTERN:
        case TK_INLINE:
            return true;
        default:
            return false;
    }
}

static bool NccTokIsTypeStartTok(const NccTok *t)
{
    if (NccTokIsTypeStart(t->kind))
    {
        return true;
    }
    return t->kind == TK_IDENT && NccFindTypedef(t->sval) != NULL;
}

static bool NccPeekIsTypeStart(void)
{
    return NccTokIsTypeStartTok(&g_peek);
}

typedef struct
{
    NccType *type;
    bool is_static;
    bool is_extern;
} NccDeclSpec;

static void NccParseDeclSpec(NccDeclSpec *ds)
{
    ds->type = NULL;
    ds->is_static = false;
    ds->is_extern = false;
    for (;;)
    {
        switch (g_tok.kind)
        {
            case TK_CONST: case TK_VOLATILE: case TK_INLINE:
                NccAdvance();
                break;
            case TK_STATIC:
                ds->is_static = true;
                NccAdvance();
                break;
            case TK_EXTERN:
                ds->is_extern = true;
                NccAdvance();
                break;
            case TK_UNSIGNED:
                NccAdvance();
                if (g_tok.kind == TK_LONG)
                {
                    NccAdvance();
                    ds->type = &g_ty_ulong;
                }
                else if (g_tok.kind == TK_SHORT)
                {
                    NccAdvance();
                    ds->type = &g_ty_ushort;
                }
                else if (g_tok.kind == TK_CHAR)
                {
                    NccAdvance();
                    ds->type = &g_ty_uchar;
                }
                else
                {
                    ds->type = &g_ty_uint;
                    if (g_tok.kind == TK_INT)
                    {
                        NccAdvance();
                    }
                }
                break;
            case TK_SIGNED:
                NccAdvance();
                if (g_tok.kind == TK_LONG)
                {
                    NccAdvance();
                    ds->type = &g_ty_long;
                }
                else if (g_tok.kind == TK_SHORT)
                {
                    NccAdvance();
                    ds->type = &g_ty_short;
                }
                else if (g_tok.kind == TK_CHAR)
                {
                    NccAdvance();
                    ds->type = &g_ty_char;
                }
                else
                {
                    ds->type = &g_ty_int;
                    if (g_tok.kind == TK_INT)
                    {
                        NccAdvance();
                    }
                }
                break;
            case TK_CHAR:
                NccAdvance();
                ds->type = &g_ty_char;
                break;
            case TK_SHORT:
                NccAdvance();
                if (g_tok.kind == TK_INT)
                {
                    NccAdvance();
                }
                ds->type = &g_ty_short;
                break;
            case TK_INT:
                NccAdvance();
                ds->type = &g_ty_int;
                break;
            case TK_LONG:
                NccAdvance();
                if (g_tok.kind == TK_LONG)
                {
                    NccAdvance();
                }
                else if (g_tok.kind == TK_INT)
                {
                    NccAdvance();
                }
                ds->type = &g_ty_long;
                break;
            case TK_VOID:
                NccAdvance();
                ds->type = &g_ty_void;
                break;
            case TK_STRUCT:
            {
                NccType *t = NccOpqType("struct");
                NccAdvance();
                if (g_tok.kind == TK_IDENT)
                {
                    strncpy(t->name, g_tok.sval, sizeof(t->name) - 1U);
                    NccAdvance();
                }
                if (g_tok.kind == TK_LBRACE)
                {
                    uint32_t depth = 0;
                    do
                    {
                        if (g_tok.kind == TK_LBRACE)
                        {
                            depth++;
                        }
                        else if (g_tok.kind == TK_RBRACE)
                        {
                            depth--;
                        }
                        if (depth > 0)
                        {
                            NccAdvance();
                        }
                    } while (depth > 0 && g_tok.kind != TK_EOF);
                    if (g_tok.kind == TK_RBRACE)
                    {
                        NccAdvance();
                    }
                }
                ds->type = t;
                break;
            }
            case TK_ENUM:
            {
                NccAdvance();
                if (g_tok.kind == TK_IDENT)
                {
                    NccAdvance();
                }
                if (g_tok.kind != TK_LBRACE)
                {
                    NccError("expected '{' in enum");
                    ds->type = &g_ty_int;
                    break;
                }
                NccAdvance();
                uint64_t next = 0;
                while (g_tok.kind != TK_RBRACE && g_tok.kind != TK_EOF)
                {
                    if (g_tok.kind != TK_IDENT)
                    {
                        NccError("expected enum constant");
                        break;
                    }
                    char name[64];
                    strncpy(name, g_tok.sval, sizeof(name) - 1U);
                    name[sizeof(name) - 1U] = '\0';
                    NccAdvance();
                    uint64_t v = next;
                    if (g_tok.kind == TK_ASSIGN)
                    {
                        NccAdvance();
                        v = NccConstExpr();
                    }
                    if (g_const_count < NCC_MAX_CONSTS)
                    {
                        NccConst *c = &g_consts[g_const_count++];
                        strncpy(c->name, name, sizeof(c->name) - 1U);
                        c->name[sizeof(c->name) - 1U] = '\0';
                        c->value = v;
                    }
                    next = v + 1;
                    if (g_tok.kind == TK_COMMA)
                    {
                        NccAdvance();
                    }
                    else
                    {
                        break;
                    }
                }
                if (g_tok.kind == TK_RBRACE)
                {
                    NccAdvance();
                }
                ds->type = &g_ty_int;
                break;
            }
            case TK_IDENT:
            {
                NccType *tt = NccFindTypedef(g_tok.sval);
                if (tt == NULL)
                {
                    return;
                }
                ds->type = tt;
                NccAdvance();
                break;
            }
            default:
                return;
        }
    }
}

static NccType *NccParsePointerSuffix(NccType *base)
{
    NccType *t = base;
    while (g_tok.kind == TK_STAR)
    {
        NccAdvance();
        t = NccPtrType(t);
    }
    return t;
}

static NccType *NccParseDeclarator(NccType *base, char *name, bool allow_array)
{
    NccType *t = NccParsePointerSuffix(base);
    name[0] = '\0';
    if (g_tok.kind != TK_IDENT)
    {
        NccError("expected identifier");
        return t;
    }
    strncpy(name, g_tok.sval, 63);
    name[63] = '\0';
    NccAdvance();
    if (allow_array)
    {
        while (g_tok.kind == TK_LBRACK)
        {
            NccAdvance();
            int len = 0;
            if (g_tok.kind != TK_RBRACK)
            {
                uint64_t v = NccConstExpr();
                len = (int)v;
                if (len <= 0)
                {
                    NccError("invalid array size");
                    len = 1;
                }
            }
            if (g_tok.kind != TK_RBRACK)
            {
                NccError("expected ']'");
            }
            else
            {
                NccAdvance();
            }
            t = NccArrType(t, len);
        }
    }
    return t;
}

static void NccParseTypedef(void)
{
    NccAdvance();
    NccDeclSpec ds;
    NccParseDeclSpec(&ds);
    if (ds.type == NULL)
    {
        NccError("expected type in typedef");
        return;
    }
    for (;;)
    {
        char name[64];
        NccType *t = NccParseDeclarator(ds.type, name, false);
        if (name[0] != '\0')
        {
            NccAddTypedef(name, t);
        }
        if (g_tok.kind == TK_COMMA)
        {
            NccAdvance();
            continue;
        }
        break;
    }
    if (g_tok.kind != TK_SEMI)
    {
        NccError("expected ';' after typedef");
    }
    else
    {
        NccAdvance();
    }
}

/* ------------------------------------------------------------------ */
/* constant expression evaluator                                       */
/* ------------------------------------------------------------------ */

typedef struct
{
    int op[2];
} NccBinLvl;

#define NCC_CONST_LEVELS 12U

static const NccBinLvl NccConstLevels[NCC_CONST_LEVELS] = {
    { { TK_OROR, 0 } },
    { { TK_ANDAND, 0 } },
    { { TK_PIPE, 0 } },
    { { TK_CARET, 0 } },
    { { TK_AMP, 0 } },
    { { TK_EQEQ, TK_NEQ } },
    { { TK_LT, TK_LE } },
    { { TK_GT, TK_GE } },
    { { TK_SHL, TK_SHR } },
    { { TK_PLUS, TK_MINUS } },
    { { TK_STAR, TK_SLASH } },
    { { TK_PERCENT, 0 } }
};

static uint64_t NccConstBinary(uint32_t level);
static uint64_t NccConstExpr(void);
static uint64_t NccConstUnary(void);
static uint64_t NccConstPrimary(void);

static uint64_t NccConstBinary(uint32_t level)
{
    if (level >= NCC_CONST_LEVELS)
    {
        return NccConstUnary();
    }
    uint64_t lhs = NccConstBinary(level + 1U);
    const NccBinLvl *l = &NccConstLevels[level];
    for (;;)
    {
        int op = g_tok.kind;
        if (op != l->op[0] && op != l->op[1])
        {
            break;
        }
        NccAdvance();
        uint64_t rhs = NccConstBinary(level + 1U);
        switch (op)
        {
            case TK_OROR: lhs = (lhs != 0 || rhs != 0) ? 1 : 0; break;
            case TK_ANDAND: lhs = (lhs != 0 && rhs != 0) ? 1 : 0; break;
            case TK_PIPE: lhs |= rhs; break;
            case TK_CARET: lhs ^= rhs; break;
            case TK_AMP: lhs &= rhs; break;
            case TK_EQEQ: lhs = (lhs == rhs) ? 1 : 0; break;
            case TK_NEQ: lhs = (lhs != rhs) ? 1 : 0; break;
            case TK_LT: lhs = (lhs < rhs) ? 1 : 0; break;
            case TK_LE: lhs = (lhs <= rhs) ? 1 : 0; break;
            case TK_GT: lhs = (lhs > rhs) ? 1 : 0; break;
            case TK_GE: lhs = (lhs >= rhs) ? 1 : 0; break;
            case TK_SHL: lhs = (rhs < 64) ? (lhs << rhs) : 0; break;
            case TK_SHR: lhs = (rhs < 64) ? (lhs >> rhs) : 0; break;
            case TK_PLUS: lhs += rhs; break;
            case TK_MINUS: lhs -= rhs; break;
            case TK_STAR: lhs *= rhs; break;
            case TK_SLASH: lhs = (rhs != 0) ? (lhs / rhs) : 0; break;
            case TK_PERCENT: lhs = (rhs != 0) ? (lhs % rhs) : 0; break;
            default: break;
        }
    }
    return lhs;
}

static uint64_t NccConstUnary(void)
{
    switch (g_tok.kind)
    {
        case TK_BANG:
            NccAdvance();
            return NccConstUnary() == 0 ? 1 : 0;
        case TK_TILDE:
            NccAdvance();
            return ~NccConstUnary();
        case TK_MINUS:
            NccAdvance();
            return 0 - NccConstUnary();
        case TK_PLUS:
            NccAdvance();
            return NccConstUnary();
        case TK_LPAREN:
        {
            NccAdvance();
            if (NccTokIsTypeStartTok(&g_tok))
            {
                NccDeclSpec ds;
                NccParseDeclSpec(&ds);
                NccType *t = NccParsePointerSuffix(ds.type != NULL ? ds.type : &g_ty_int);
                if (g_tok.kind != TK_RPAREN)
                {
                    NccError("expected ')' in cast");
                }
                else
                {
                    NccAdvance();
                }
                (void)t;
            }
            else
            {
                uint64_t v = NccConstExpr();
                if (g_tok.kind != TK_RPAREN)
                {
                    NccError("expected ')'");
                }
                else
                {
                    NccAdvance();
                }
                return v;
            }
            return NccConstUnary();
        }
        case TK_SIZEOF:
        {
            NccAdvance();
            if (g_tok.kind == TK_LPAREN && NccPeekIsTypeStart())
            {
                NccAdvance();
                NccDeclSpec ds;
                NccParseDeclSpec(&ds);
                NccType *t = NccParsePointerSuffix(ds.type != NULL ? ds.type : &g_ty_int);
                if (g_tok.kind != TK_RPAREN)
                {
                    NccError("expected ')' in sizeof");
                }
                else
                {
                    NccAdvance();
                }
                return (uint64_t)t->size;
            }
            if (g_tok.kind == TK_IDENT)
            {
                NccVar *v = NccFindLocal(g_tok.sval);
                if (v != NULL)
                {
                    NccAdvance();
                    return (uint64_t)v->type->size;
                }
                int gi = NccFindGlobal(g_tok.sval);
                if (gi >= 0)
                {
                    NccAdvance();
                    return (uint64_t)g_globals[gi].type->size;
                }
                NccType *tt = NccFindTypedef(g_tok.sval);
                if (tt != NULL)
                {
                    NccAdvance();
                    return (uint64_t)tt->size;
                }
                NccError("unknown name in sizeof");
                return 4;
            }
            NccError("sizeof requires a type");
            return 4;
        }
        default:
            return NccConstPrimary();
    }
}

static uint64_t NccConstPrimary(void)
{
    switch (g_tok.kind)
    {
        case TK_NUM:
        {
            uint64_t v = g_tok.ival;
            NccAdvance();
            return v;
        }
        case TK_CHR:
        {
            uint64_t v = g_tok.ival;
            NccAdvance();
            return v;
        }
        case TK_STR:
        {
            uint64_t v = NccMakeString(g_strbuf, g_tok.str_len);
            NccAdvance();
            return v;
        }
        case TK_IDENT:
        {
            for (uint32_t i = 0; i < g_const_count; i++)
            {
                if (strcmp(g_consts[i].name, g_tok.sval) == 0)
                {
                    uint64_t v = g_consts[i].value;
                    NccAdvance();
                    return v;
                }
            }
            NccError("undefined name in constant expression");
            NccAdvance();
            return 0;
        }
        default:
            NccError("expected constant expression");
            return 0;
    }
}

static uint64_t NccConstExpr(void)
{
    uint64_t a = NccConstBinary(0);
    if (g_tok.kind == TK_QUESTION)
    {
        NccAdvance();
        uint64_t b = NccConstExpr();
        if (g_tok.kind != TK_COLON)
        {
            NccError("expected ':' in conditional");
            return a;
        }
        NccAdvance();
        uint64_t c = NccConstExpr();
        return a != 0 ? b : c;
    }
    return a;
}

/* ------------------------------------------------------------------ */
/* variable registration + declarations                                */
/* ------------------------------------------------------------------ */

static NccVar *NccRegisterLocal(const char *name, NccType *t)
{
    if (g_locals_count >= NCC_MAX_LOCALS)
    {
        NccError("too many locals");
        return NULL;
    }
    int a = NccTypeAlign(t);
    g_local_off = (g_local_off - t->size) & ~(a - 1);
    if (g_local_off < g_local_min_off)
    {
        g_local_min_off = g_local_off;
    }
    NccVar *v = &g_locals[g_locals_count++];
    strncpy(v->name, name, sizeof(v->name) - 1U);
    v->name[sizeof(v->name) - 1U] = '\0';
    v->type = t;
    v->kind = VAR_LOCAL;
    v->offset = g_local_off;
    v->addr = 0;
    return v;
}

static NccVar *NccRegisterGlobal(const char *name, NccType *t)
{
    if (g_globals_count >= NCC_MAX_GLOBALS)
    {
        NccError("too many globals");
        return NULL;
    }
    uint64_t addr = NccDataAlloc((uint32_t)t->size, (uint32_t)NccTypeAlign(t));
    NccVar *v = &g_globals[g_globals_count++];
    strncpy(v->name, name, sizeof(v->name) - 1U);
    v->name[sizeof(v->name) - 1U] = '\0';
    v->type = t;
    v->kind = VAR_GLOBAL;
    v->offset = 0;
    v->addr = addr;
    return v;
}

static void NccWriteConstData(uint64_t vaddr, NccType *t, uint64_t value)
{
    uint8_t tmp[8];
    uint32_t n = t->size > 8 ? 8U : (uint32_t)t->size;
    if (n == 0)
    {
        return;
    }
    memcpy(tmp, &value, n);
    NccDataWrite(vaddr, tmp, n);
}

static void NccParseGlobalDecl(NccDeclSpec *ds, const char *name, NccType *t)
{
    if (ds->is_extern)
    {
        if (g_tok.kind == TK_ASSIGN)
        {
            NccError("extern variable cannot have an initializer");
            return;
        }
        if (NccFindGlobal(name) < 0 && g_globals_count < NCC_MAX_GLOBALS)
        {
            NccVar *v = &g_globals[g_globals_count++];
            strncpy(v->name, name, sizeof(v->name) - 1U);
            v->name[sizeof(v->name) - 1U] = '\0';
            v->type = t;
            v->kind = VAR_EXTERN;
            v->offset = 0;
            v->addr = 0;
        }
        return;
    }
    if (NccFindGlobal(name) >= 0)
    {
        NccError("duplicate global variable");
        return;
    }
    NccVar *v = NccRegisterGlobal(name, t);
    if (v == NULL)
    {
        return;
    }
    if (g_tok.kind == TK_ASSIGN)
    {
        NccAdvance();
        if (t->kind == TY_ARR)
        {
            if (g_tok.kind == TK_STR)
            {
                uint32_t n = g_tok.str_len + 1U;
                if (t->arr_len <= 0)
                {
                    v->type = NccArrType(t->elem, (int)n);
                }
                else if ((uint32_t)t->arr_len < n)
                {
                    n = (uint32_t)t->arr_len;
                }
                NccDataWrite(v->addr, g_strbuf, n - 1U);
                NccAdvance();
            }
            else
            {
                NccError("array initializer must be a string literal");
            }
        }
        else
        {
            uint64_t val = NccConstExpr();
            NccWriteConstData(v->addr, t, val);
        }
    }
}

/* ------------------------------------------------------------------ */
/* expressions                                                         */
/* ------------------------------------------------------------------ */

static int NccArgReg(uint32_t i)
{
    switch (i)
    {
        case 0: return 7;
        case 1: return 6;
        case 2: return 2;
        case 3: return 1;
        case 4: return 8;
        default: return 9;
    }
}

static void NccParsePrimary(void);
static void NccParseExpr(int mode);

static void NccParsePostfix(void)
{
    NccParsePrimary();
    for (;;)
    {
        switch (g_tok.kind)
        {
            case TK_LBRACK:
            {
                NccType *pt = g_last_type;
                bool baseIsAddr = g_last_expr_is_addr;
                NccEmitPushRax();
                NccAdvance();
                NccParseExpr(MODE_RV);
                NccEmitMovRaxToRcx();
                NccEmitPop(0);
                int esz = 1;
                if (pt != NULL && (pt->kind == TY_PTR || pt->kind == TY_ARR))
                {
                    esz = pt->elem != NULL ? pt->elem->size : 1;
                    if (pt->kind == TY_PTR && baseIsAddr)
                    {
                        NccEmitLoadRaxFromRaxPtr(&g_ty_ulong);
                    }
                }
                if (esz != 1)
                {
                    NccEmitImulRcxImm32((uint32_t)esz);
                }
                NccEmitAddRaxRcx();
                if (g_tok.kind != TK_RBRACK)
                {
                    NccError("expected ']'");
                }
                else
                {
                    NccAdvance();
                }
                if (pt != NULL && (pt->kind == TY_PTR || pt->kind == TY_ARR))
                {
                    g_last_type = pt->elem;
                }
                else
                {
                    g_last_type = &g_ty_int;
                }
                g_last_expr_is_addr = true;
                continue;
            }
            case TK_LPAREN:
            {
                char callee[80];
                strncpy(callee, g_call_name, sizeof(callee) - 1U);
                callee[sizeof(callee) - 1U] = '\0';
                if (!g_last_is_func)
                {
                    NccError("called object is not a function");
                    g_last_is_func = false;
                }
                NccAdvance();
                uint32_t n = 0;
                if (g_tok.kind != TK_RPAREN)
                {
                    for (;;)
                    {
                        NccParseExpr(MODE_RV);
                        NccEmitPushRax();
                        n++;
                        if (g_tok.kind == TK_COMMA)
                        {
                            NccAdvance();
                            continue;
                        }
                        break;
                    }
                }
                if (g_tok.kind != TK_RPAREN)
                {
                    NccError("expected ')' after arguments");
                }
                else
                {
                    NccAdvance();
                }
                if (n > NCC_MAX_ARGS)
                {
                    NccError("too many arguments (max 6)");
                    n = NCC_MAX_ARGS;
                }
                for (uint32_t i = n; i > 0; i--)
                {
                    NccEmitPop(NccArgReg(i - 1U));
                }
                NccEmitCallRef(callee);
                NccEmitTrunc(g_last_type);
                g_last_expr_is_addr = false;
                g_last_is_func = false;
                continue;
            }
            case TK_INC:
            case TK_DEC:
            {
                int inc = g_tok.kind == TK_INC;
                if (!g_last_expr_is_addr)
                {
                    NccError("invalid lvalue for '++'/'--'");
                }
                NccType *t = g_last_type;
                NccAdvance();
                if (t == NULL)
                {
                    t = &g_ty_int;
                }
                NccEmitMovRaxToRcx();
                NccEmitLoadRaxFromRcxPtr(t);
                NccEmitPushRax();
                NccEmitPushRcx();
                if (t->kind == TY_PTR)
                {
                    NccEmitMovRcxImm64((uint64_t)t->elem->size);
                }
                else
                {
                    NccEmitMovRcxImm64(1);
                }
                NccEmitBinop(inc ? TK_PLUS : TK_MINUS, NccTypeIs64(t));
                NccEmitPop(1);
                NccEmitStoreRaxToRcxPtr(t);
                NccEmitPop(1);
                NccEmitMovRcxToRax();
                g_last_expr_is_addr = false;
                continue;
            }
            case TK_DOT:
            case TK_ARROW:
                NccError("struct member access is not supported");
                g_last_expr_is_addr = false;
                NccAdvance();
                continue;
            default:
                return;
        }
    }
}

static void NccParsePrimary(void)
{
    switch (g_tok.kind)
    {
        case TK_NUM:
        {
            bool is_long = g_tok.sval[0] == 'l' || g_tok.ival > 0xFFFFFFFFULL;
            bool is_unsigned = g_tok.sval[0] == 'u';
            NccEmitMovRaxImm(g_tok.ival, is_long);
            if (is_long)
            {
                g_last_type = is_unsigned ? &g_ty_ulong : &g_ty_long;
            }
            else
            {
                g_last_type = is_unsigned ? &g_ty_uint : &g_ty_int;
            }
            g_last_expr_is_addr = false;
            g_last_is_func = false;
            NccAdvance();
            return;
        }
        case TK_CHR:
            NccEmitMovRaxImm(g_tok.ival, false);
            g_last_type = &g_ty_int;
            g_last_expr_is_addr = false;
            g_last_is_func = false;
            NccAdvance();
            return;
        case TK_STR:
            NccMakeString(g_strbuf, g_tok.str_len);
            NccRefAbs(g_str_name);
            g_last_type = NccPtrType(&g_ty_char);
            g_last_expr_is_addr = false;
            g_last_is_func = false;
            NccAdvance();
            return;
        case TK_IDENT:
        {
            const char *name = g_tok.sval;
            for (uint32_t i = 0; i < g_const_count; i++)
            {
                if (strcmp(g_consts[i].name, name) == 0)
                {
                    NccEmitMovRaxImm(g_consts[i].value, false);
                    g_last_type = &g_ty_int;
                    g_last_expr_is_addr = false;
                    g_last_is_func = false;
                    NccAdvance();
                    return;
                }
            }
            NccVar *v = NccFindLocal(name);
            if (v != NULL)
            {
                NccEmitLeaLocal(v->offset);
                g_last_type = v->type;
                g_last_expr_is_addr = true;
                g_last_is_func = false;
                NccAdvance();
                return;
            }
            int gi = NccFindGlobal(name);
            if (gi >= 0)
            {
                NccRefAbs(g_globals[gi].name);
                g_last_type = g_globals[gi].type;
                g_last_expr_is_addr = true;
                g_last_is_func = false;
                NccAdvance();
                return;
            }
            if (NccFindTypedef(name) != NULL)
            {
                NccError("typedef name used as a value");
                g_last_type = &g_ty_int;
                g_last_expr_is_addr = false;
                g_last_is_func = false;
                NccAdvance();
                return;
            }
            strncpy(g_call_name, name, sizeof(g_call_name) - 1U);
            g_call_name[sizeof(g_call_name) - 1U] = '\0';
            int fi = NccFindFunc(name);
            g_last_type = fi >= 0 ? g_funcs[fi].ret : &g_ty_int;
            if (g_last_type == NULL)
            {
                g_last_type = &g_ty_int;
            }
            g_last_expr_is_addr = false;
            g_last_is_func = true;
            NccAdvance();
            return;
        }
        default:
            NccError("expected expression");
            g_last_type = &g_ty_int;
            g_last_expr_is_addr = false;
            g_last_is_func = false;
            return;
    }
}

static void NccParseSizeof(void)
{
    NccAdvance();
    int size = 4;
    if (g_tok.kind == TK_LPAREN && NccPeekIsTypeStart())
    {
        NccAdvance();
        NccDeclSpec ds;
        NccParseDeclSpec(&ds);
        NccType *t = NccParsePointerSuffix(ds.type != NULL ? ds.type : &g_ty_int);
        if (g_tok.kind != TK_RPAREN)
        {
            NccError("expected ')' in sizeof");
        }
        else
        {
            NccAdvance();
        }
        size = t->size;
    }
    else if (g_tok.kind == TK_LPAREN)
    {
        NccAdvance();
        NccVar *v = NULL;
        int gi = -1;
        if (g_tok.kind == TK_IDENT)
        {
            v = NccFindLocal(g_tok.sval);
            if (v == NULL)
            {
                gi = NccFindGlobal(g_tok.sval);
            }
        }
        if (v != NULL || gi >= 0)
        {
            size = v != NULL ? v->type->size : g_globals[gi].type->size;
            NccAdvance();
        }
        else
        {
            NccParseExpr(MODE_RV);
            size = g_last_type != NULL ? g_last_type->size : 4;
        }
        if (g_tok.kind != TK_RPAREN)
        {
            NccError("expected ')' in sizeof");
        }
        else
        {
            NccAdvance();
        }
    }
    else if (g_tok.kind == TK_IDENT)
    {
        NccVar *v = NccFindLocal(g_tok.sval);
        if (v != NULL)
        {
            size = v->type->size;
        }
        else
        {
            int gi = NccFindGlobal(g_tok.sval);
            if (gi >= 0)
            {
                size = g_globals[gi].type->size;
            }
            else
            {
                NccError("unknown name in sizeof");
            }
        }
        NccAdvance();
    }
    else
    {
        NccError("sizeof requires a type");
    }
    NccEmitMovRaxImm((uint64_t)size, false);
    g_last_type = &g_ty_int;
    g_last_expr_is_addr = false;
    g_last_is_func = false;
}

static int NccBinopOfAssignOp(int op);
static bool NccIsAssignOp(int kind)
{
    return kind == TK_ASSIGN || NccBinopOfAssignOp(kind) != 0;
}

static void NccParseUnary(int mode)
{
    switch (g_tok.kind)
    {
        case TK_BANG:
            NccAdvance();
            NccParseUnary(MODE_RV);
            NccEmitTestRax();
            NccEmitSetCC(0x94);
            g_last_type = &g_ty_int;
            g_last_expr_is_addr = false;
            return;
        case TK_TILDE:
            NccAdvance();
            NccParseUnary(MODE_RV);
            NccEmitNot(NccTypeIs64(g_last_type));
            g_last_expr_is_addr = false;
            return;
        case TK_MINUS:
            NccAdvance();
            NccParseUnary(MODE_RV);
            NccEmitNeg(NccTypeIs64(g_last_type));
            g_last_expr_is_addr = false;
            return;
        case TK_PLUS:
            NccAdvance();
            NccParseUnary(MODE_RV);
            g_last_expr_is_addr = false;
            return;
        case TK_STAR:
        {
            NccAdvance();
            NccParseUnary(MODE_RV);
            NccType *pt = g_last_type;
            if (pt != NULL && (pt->kind == TY_PTR || pt->kind == TY_ARR))
            {
                g_last_type = pt->elem;
            }
            else
            {
                NccError("dereference of non-pointer");
                g_last_type = &g_ty_int;
            }
            if (mode != MODE_LV && !NccIsAssignOp(g_tok.kind))
            {
                NccEmitLoadRaxFromRaxPtr(g_last_type);
                g_last_expr_is_addr = false;
            }
            else
            {
                g_last_expr_is_addr = true;
            }
            return;
        }
        case TK_AMP:
        {
            NccAdvance();
            NccParseUnary(MODE_LV);
            if (!g_last_expr_is_addr)
            {
                NccError("invalid lvalue for '&'");
            }
            g_last_type = NccPtrType(g_last_type);
            g_last_expr_is_addr = false;
            return;
        }
        case TK_INC:
        case TK_DEC:
        {
            int inc = g_tok.kind == TK_INC;
            NccAdvance();
            NccParseUnary(MODE_LV);
            if (!g_last_expr_is_addr)
            {
                NccError("invalid lvalue for '++'/'--'");
                g_last_type = &g_ty_int;
            }
            NccType *t = g_last_type;
            NccEmitMovRaxToRcx();
            NccEmitLoadRaxFromRcxPtr(t);
            NccEmitPushRcx();
            if (t->kind == TY_PTR)
            {
                NccEmitMovRcxImm64((uint64_t)t->elem->size);
            }
            else
            {
                NccEmitMovRcxImm64(1);
            }
            NccEmitBinop(inc ? TK_PLUS : TK_MINUS, NccTypeIs64(t));
            NccEmitPop(1);
            NccEmitStoreRaxToRcxPtr(t);
            g_last_expr_is_addr = false;
            return;
        }
        case TK_SIZEOF:
            NccParseSizeof();
            return;
        case TK_LPAREN:
        {
            NccAdvance();
            if (NccTokIsTypeStartTok(&g_tok))
            {
                NccDeclSpec ds;
                NccParseDeclSpec(&ds);
                NccType *t = NccParsePointerSuffix(ds.type != NULL ? ds.type : &g_ty_int);
                if (g_tok.kind != TK_RPAREN)
                {
                    NccError("expected ')' in cast");
                }
                else
                {
                    NccAdvance();
                }
                NccParseUnary(MODE_RV);
                NccEmitTrunc(t);
                g_last_type = t;
                g_last_expr_is_addr = false;
                return;
            }
            NccParseExpr(mode);
            if (g_tok.kind != TK_RPAREN)
            {
                NccError("expected ')'");
            }
            else
            {
                NccAdvance();
            }
            return;
        }
        default:
            NccParsePostfix();
            if (g_last_is_func)
            {
                NccError("function name used as a value");
                g_last_is_func = false;
                g_last_expr_is_addr = false;
                return;
            }
            if (mode == MODE_RV && g_last_expr_is_addr &&
                !NccIsAssignOp(g_tok.kind))
            {
                if (g_last_type != NULL && g_last_type->kind == TY_ARR)
                {
                    g_last_type = NccPtrType(g_last_type->elem);
                    g_last_expr_is_addr = false;
                }
                else
                {
                    NccEmitLoadRaxFromRaxPtr(g_last_type);
                    g_last_expr_is_addr = false;
                }
            }
            return;
    }
}

static void NccParseMul(int mode)
{
    NccParseUnary(mode);
    while (g_tok.kind == TK_STAR || g_tok.kind == TK_SLASH ||
           g_tok.kind == TK_PERCENT)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        int op = g_tok.kind;
        NccType *t1 = g_last_type;
        NccEmitPushRax();
        NccAdvance();
        NccParseUnary(MODE_RV);
        NccType *t2 = g_last_type;
        bool w64 = NccTypeIs64(t1) || NccTypeIs64(t2);
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        if (t1 != NULL && (t1->kind == TY_PTR || t1->kind == TY_ARR))
        {
            NccError("invalid pointer arithmetic");
            g_last_type = t1;
            continue;
        }
        if (op == TK_STAR)
        {
            NccEmitBinop(TK_STAR, w64);
        }
        else
        {
            NccEmitDiv(op == TK_PERCENT, w64,
                       NccTypeIsUnsigned(t1) || NccTypeIsUnsigned(t2));
        }
        g_last_type = t1;
        g_last_expr_is_addr = false;
    }
}

static void NccParseAdd(int mode)
{
    NccParseMul(mode);
    while (g_tok.kind == TK_PLUS || g_tok.kind == TK_MINUS)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        int op = g_tok.kind;
        NccType *t1 = g_last_type;
        NccEmitPushRax();
        NccAdvance();
        NccParseMul(MODE_RV);
        NccType *t2 = g_last_type;
        bool w64 = NccTypeIs64(t1) || NccTypeIs64(t2);
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        bool ptr1 = t1 != NULL && (t1->kind == TY_PTR || t1->kind == TY_ARR);
        bool ptr2 = t2 != NULL && (t2->kind == TY_PTR || t2->kind == TY_ARR);
        if (ptr1 && ptr2)
        {
            NccError("invalid pointer arithmetic");
            g_last_type = t1;
            continue;
        }
        if (ptr1)
        {
            NccEmitImulRcxImm32((uint32_t)t1->elem->size);
        }
        else if (ptr2)
        {
            if (op == TK_MINUS)
            {
                NccError("invalid pointer arithmetic");
                g_last_type = t2;
                continue;
            }
            NccEmitImulRaxImm32((uint32_t)t2->elem->size);
        }
        NccEmitBinop(op, w64);
        g_last_type = ptr1 ? t1 : (ptr2 ? t2 : t1);
        g_last_expr_is_addr = false;
    }
}

static void NccParseShift(int mode)
{
    NccParseAdd(mode);
    while (g_tok.kind == TK_SHL || g_tok.kind == TK_SHR)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        int op = g_tok.kind;
        NccType *t1 = g_last_type;
        NccEmitPushRax();
        NccAdvance();
        NccParseAdd(MODE_RV);
        NccType *t2 = g_last_type;
        bool w64 = NccTypeIs64(t1) || NccTypeIs64(t2);
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        NccEmitBinop(op, w64);
        g_last_type = t1;
        g_last_expr_is_addr = false;
    }
}

static void NccParseRelational(int mode)
{
    NccParseShift(mode);
    while (g_tok.kind == TK_LT || g_tok.kind == TK_GT ||
           g_tok.kind == TK_LE || g_tok.kind == TK_GE)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        int op = g_tok.kind;
        NccType *t1 = g_last_type;
        NccEmitPushRax();
        NccAdvance();
        NccParseShift(MODE_RV);
        NccType *t2 = g_last_type;
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        NccEmitCmp(op, NccTypeIsUnsigned(t1) || NccTypeIsUnsigned(t2));
        g_last_type = &g_ty_int;
        g_last_expr_is_addr = false;
    }
}

static void NccParseEquality(int mode)
{
    NccParseRelational(mode);
    while (g_tok.kind == TK_EQEQ || g_tok.kind == TK_NEQ)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        int op = g_tok.kind;
        NccType *t1 = g_last_type;
        NccEmitPushRax();
        NccAdvance();
        NccParseRelational(MODE_RV);
        NccType *t2 = g_last_type;
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        NccEmitCmp(op, NccTypeIsUnsigned(t1) || NccTypeIsUnsigned(t2));
        g_last_type = &g_ty_int;
        g_last_expr_is_addr = false;
    }
}

static void NccParseBitAnd(int mode)
{
    NccParseEquality(mode);
    while (g_tok.kind == TK_AMP)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        NccType *t1 = g_last_type;
        NccEmitPushRax();
        NccAdvance();
        NccParseEquality(MODE_RV);
        NccType *t2 = g_last_type;
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        NccEmitBinop(TK_AMP, NccTypeIs64(t1) || NccTypeIs64(t2));
        g_last_type = t1;
        g_last_expr_is_addr = false;
    }
}

static void NccParseBitXor(int mode)
{
    NccParseBitAnd(mode);
    while (g_tok.kind == TK_CARET)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        NccType *t1 = g_last_type;
        NccEmitPushRax();
        NccAdvance();
        NccParseBitAnd(MODE_RV);
        NccType *t2 = g_last_type;
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        NccEmitBinop(TK_CARET, NccTypeIs64(t1) || NccTypeIs64(t2));
        g_last_type = t1;
        g_last_expr_is_addr = false;
    }
}

static void NccParseBitOr(int mode)
{
    NccParseBitXor(mode);
    while (g_tok.kind == TK_PIPE)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        NccType *t1 = g_last_type;
        NccEmitPushRax();
        NccAdvance();
        NccParseBitXor(MODE_RV);
        NccType *t2 = g_last_type;
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        NccEmitBinop(TK_PIPE, NccTypeIs64(t1) || NccTypeIs64(t2));
        g_last_type = t1;
        g_last_expr_is_addr = false;
    }
}

static void NccParseLogAnd(int mode)
{
    NccParseBitOr(mode);
    while (g_tok.kind == TK_ANDAND)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        uint32_t l_false = NccNewLabel();
        uint32_t l_end = NccNewLabel();
        NccAdvance();
        NccEmitTestRax();
        NccJz(l_false);
        NccParseBitOr(MODE_RV);
        NccEmitTestRax();
        NccJz(l_false);
        NccEmitMovEaxImm(1);
        NccJmp(l_end);
        NccDefLabel(l_false);
        NccEmitMovEaxImm(0);
        NccDefLabel(l_end);
        g_last_type = &g_ty_int;
        g_last_expr_is_addr = false;
    }
}

static void NccParseLogOr(int mode)
{
    NccParseLogAnd(mode);
    while (g_tok.kind == TK_OROR)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        uint32_t l_true = NccNewLabel();
        uint32_t l_end = NccNewLabel();
        NccAdvance();
        NccEmitTestRax();
        NccJnz(l_true);
        NccParseLogAnd(MODE_RV);
        NccEmitTestRax();
        NccJnz(l_true);
        NccEmitMovEaxImm(0);
        NccJmp(l_end);
        NccDefLabel(l_true);
        NccEmitMovEaxImm(1);
        NccDefLabel(l_end);
        g_last_type = &g_ty_int;
        g_last_expr_is_addr = false;
    }
}

static void NccParseTernary(int mode)
{
    NccParseLogOr(mode);
    if (g_tok.kind == TK_QUESTION)
    {
        if (mode == MODE_LV)
        {
            NccError("invalid lvalue");
            return;
        }
        uint32_t l_false = NccNewLabel();
        uint32_t l_end = NccNewLabel();
        NccAdvance();
        NccEmitTestRax();
        NccJz(l_false);
        NccParseExpr(MODE_RV);
        if (g_tok.kind != TK_COLON)
        {
            NccError("expected ':' in conditional");
            return;
        }
        NccAdvance();
        NccJmp(l_end);
        NccDefLabel(l_false);
        NccParseExpr(MODE_RV);
        NccDefLabel(l_end);
        g_last_expr_is_addr = false;
    }
}

static int NccBinopOfAssignOp(int op)
{
    switch (op)
    {
        case TK_PLUSEQ: return TK_PLUS;
        case TK_MINUSEQ: return TK_MINUS;
        case TK_STAREQ: return TK_STAR;
        case TK_SLASHEQ: return TK_SLASH;
        case TK_PERCENTEQ: return TK_PERCENT;
        case TK_AMPEQ: return TK_AMP;
        case TK_PIPEEQ: return TK_PIPE;
        case TK_CARETEQ: return TK_CARET;
        case TK_SHLASSIGN: return TK_SHL;
        case TK_SHRASSIGN: return TK_SHR;
        default: return 0;
    }
}

static void NccParseAssign(int mode)
{
    NccParseTernary(mode);
    int op = g_tok.kind;
    if (op != TK_ASSIGN && NccBinopOfAssignOp(op) == 0)
    {
        return;
    }
    if (mode == MODE_LV)
    {
        NccError("invalid lvalue in assignment");
        return;
    }
    if (!g_last_expr_is_addr)
    {
        NccError("invalid lvalue in assignment");
        return;
    }
    NccType *t = g_last_type;
    if (t == NULL)
    {
        t = &g_ty_int;
    }
    NccEmitPushRax();
    NccAdvance();
    NccParseAssign(MODE_RV);
    if (op == TK_ASSIGN)
    {
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        NccEmitSwap();
        NccEmitStoreRaxToRcxPtr(t);
    }
    else
    {
        NccEmitMovRaxToRcx();
        NccEmitPop(0);
        NccEmitPushRcx();
        NccEmitMovRaxToRcx();
        NccEmitLoadRaxFromRcxPtr(t);
        NccEmitPop(1);
        if (t->kind == TY_PTR)
        {
            NccEmitImulRcxImm32((uint32_t)t->elem->size);
        }
        NccEmitBinop(NccBinopOfAssignOp(op), NccTypeIs64(t));
        NccEmitPop(1);
        NccEmitStoreRaxToRcxPtr(t);
    }
    g_last_type = t;
    g_last_expr_is_addr = false;
}

static void NccParseExpr(int mode)
{
    NccParseAssign(mode);
}

/* ------------------------------------------------------------------ */
/* statements                                                          */
/* ------------------------------------------------------------------ */

static void NccParseStmt(void);

static void NccParseLocalDecl(bool expect_semi)
{
    NccDeclSpec ds;
    NccParseDeclSpec(&ds);
    if (ds.type == NULL)
    {
        NccError("expected type in declaration");
        if (expect_semi && g_tok.kind == TK_SEMI)
        {
            NccAdvance();
        }
        return;
    }
    for (;;)
    {
        char name[64];
        NccType *t = NccParseDeclarator(ds.type, name, true);
        if (t->size <= 0)
        {
            NccError("cannot declare variable of this type");
        }
        NccVar *v = NULL;
        if (name[0] != '\0')
        {
            v = NccRegisterLocal(name, t);
        }
        if (g_tok.kind == TK_ASSIGN)
        {
            NccAdvance();
            if (t->kind == TY_ARR)
            {
                if (g_tok.kind == TK_STR)
                {
                    uint32_t len = g_tok.str_len;
                    if (t->arr_len <= 0)
                    {
                        NccType *nt = NccArrType(t->elem, (int)len + 1);
                        if (v != NULL)
                        {
                            v->type = nt;
                        }
                        t = nt;
                    }
                    NccMakeString(g_strbuf, len);
                    NccRefAbsToReg(6, g_str_name);
                    NccEmitLeaToReg(7, v != NULL ? v->offset : 0);
                    uint32_t copy_len = len + 1U;
                    if (t->arr_len > 0 && (uint32_t)t->arr_len < copy_len)
                    {
                        copy_len = (uint32_t)t->arr_len;
                    }
                    NccTextByte(0x48); NccTextByte(0xC7); NccTextByte(0xC1);
                    NccTextU32(copy_len);
                    NccTextByte(0xF3); NccTextByte(0xA4);
                    NccAdvance();
                    g_last_expr_is_addr = false;
                }
                else
                {
                    NccError("array initializer must be a string literal");
                }
            }
            else
            {
                NccParseExpr(MODE_RV);
                if (v != NULL)
                {
                    NccEmitPushRax();
                    NccEmitLeaLocal(v->offset);
                    NccEmitPop(1);
                    NccEmitSwap();
                    NccEmitStoreRaxToRcxPtr(t);
                }
            }
        }
        if (g_tok.kind == TK_COMMA)
        {
            NccAdvance();
            continue;
        }
        break;
    }
    if (expect_semi && g_tok.kind == TK_SEMI)
    {
        NccAdvance();
    }
}

/* ------------------------------------------------------------------ */
/* inline assembly:  asm { <raw n64a source> }                          */
/* ------------------------------------------------------------------ */

static void NccEmitAsmBlock(const char *src, uint32_t len)
{
    N64aDiag diag;
    N64aDiagInit(&diag, "<asm>");

    char *buf = (char *)kmalloc((uint64_t)len + 1U);
    if (buf == NULL)
    {
        NccError("asm{}: out of memory");
        return;
    }
    memcpy(buf, src, len);
    buf[len] = '\0';

    uint8_t *obj = NULL;
    uint32_t objsize = 0;
    bool ok = N64aAssemble("<asm>", buf, (unsigned long long)len,
                           &obj, &objsize, &diag);
    kfree(buf);
    if (!ok)
    {
        TerminalPrintLine32(diag.Message, ERR_COLOR);
        NccError("asm{}: assembly failed");
        return;
    }
    if (objsize < N64O_HDRSZ)
    {
        NccError("asm{}: bad object");
        kfree(obj);
        return;
    }

    uint32_t textsize = *(const uint32_t *)(obj + 16);
    uint32_t reloccount = *(const uint32_t *)(obj + 28);
    if (reloccount != 0U)
    {
        NccError("asm{}: external references not supported yet");
        kfree(obj);
        return;
    }
    if (textsize > 0U)
    {
        NccTextBytes(obj + N64O_HDRSZ, textsize);
    }
    kfree(obj);
}

static void NccParseStmt(void)
{
    switch (g_tok.kind)
    {
        case TK_EOF:
            return;
        case TK_SEMI:
            NccAdvance();
            break;
        case TK_LBRACE:
            NccAdvance();
            NccScopePush();
            while (g_tok.kind != TK_RBRACE && g_tok.kind != TK_EOF)
            {
                NccParseStmt();
            }
            if (g_tok.kind == TK_RBRACE)
            {
                NccAdvance();
            }
            NccScopePop();
            break;
        case TK_ASM:
        {
            NccAdvance(); /* consume 'asm' */
            if (g_tok.kind != TK_LBRACE)
            {
                NccError("expected '{' after asm");
                return;
            }
            /* g_lex_p now points right after the opening '{' */
            const char *start = g_lex_p;
            const char *p = start;
            int depth = 1;
            while (*p != '\0' && depth > 0)
            {
                if (*p == '{') depth++;
                else if (*p == '}')
                {
                    depth--;
                    if (depth == 0) break;
                }
                p++;
            }
            if (depth != 0)
            {
                NccError("unterminated asm{} block");
                return;
            }
            uint32_t bodylen = (uint32_t)(p - start);
            NccEmitAsmBlock(start, bodylen);
            /* resume lexing right after the closing '}' */
            g_lex_p = p + 1;
            g_peek_valid = false;
            NccAdvance();
            break;
        }
        case TK_IF:
        {
            NccAdvance();
            if (g_tok.kind != TK_LPAREN)
            {
                NccError("expected '(' after if");
                return;
            }
            NccAdvance();
            NccParseExpr(MODE_RV);
            if (g_tok.kind != TK_RPAREN)
            {
                NccError("expected ')' after if condition");
                return;
            }
            NccAdvance();
            uint32_t l_else = NccNewLabel();
            uint32_t l_end = NccNewLabel();
            NccEmitTestRax();
            NccJz(l_else);
            NccParseStmt();
            if (g_tok.kind == TK_ELSE)
            {
                NccAdvance();
                NccJmp(l_end);
                NccDefLabel(l_else);
                NccParseStmt();
                NccDefLabel(l_end);
            }
            else
            {
                NccDefLabel(l_else);
            }
            break;
        }
        case TK_WHILE:
        {
            NccAdvance();
            uint32_t l_cond = NccNewLabel();
            uint32_t l_end = NccNewLabel();
            NccDefLabel(l_cond);
            if (g_tok.kind != TK_LPAREN)
            {
                NccError("expected '(' after while");
                return;
            }
            NccAdvance();
            NccParseExpr(MODE_RV);
            if (g_tok.kind != TK_RPAREN)
            {
                NccError("expected ')' after while condition");
                return;
            }
            NccAdvance();
            NccEmitTestRax();
            NccJz(l_end);
            if (g_loop_depth < NCC_MAX_LOOP)
            {
                g_loop_break[g_loop_depth] = (int)l_end;
                g_loop_cont[g_loop_depth] = (int)l_cond;
                g_loop_depth++;
            }
            NccParseStmt();
            if (g_loop_depth > 0)
            {
                g_loop_depth--;
            }
            NccJmp(l_cond);
            NccDefLabel(l_end);
            break;
        }
        case TK_DO:
        {
            NccAdvance();
            uint32_t l_start = NccNewLabel();
            uint32_t l_cont = NccNewLabel();
            uint32_t l_end = NccNewLabel();
            NccDefLabel(l_start);
            if (g_loop_depth < NCC_MAX_LOOP)
            {
                g_loop_break[g_loop_depth] = (int)l_end;
                g_loop_cont[g_loop_depth] = (int)l_cont;
                g_loop_depth++;
            }
            NccParseStmt();
            if (g_loop_depth > 0)
            {
                g_loop_depth--;
            }
            NccDefLabel(l_cont);
            if (g_tok.kind != TK_WHILE)
            {
                NccError("expected 'while' after do body");
                return;
            }
            NccAdvance();
            if (g_tok.kind != TK_LPAREN)
            {
                NccError("expected '(' after do-while");
                return;
            }
            NccAdvance();
            NccParseExpr(MODE_RV);
            if (g_tok.kind != TK_RPAREN)
            {
                NccError("expected ')' in do-while");
                return;
            }
            NccAdvance();
            NccEmitTestRax();
            NccJnz(l_start);
            NccDefLabel(l_end);
            if (g_tok.kind == TK_SEMI)
            {
                NccAdvance();
            }
            break;
        }
        case TK_FOR:
        {
            NccAdvance();
            if (g_tok.kind != TK_LPAREN)
            {
                NccError("expected '(' after for");
                return;
            }
            NccAdvance();
            if (g_tok.kind != TK_SEMI)
            {
                if (NccTokIsTypeStartTok(&g_tok))
                {
                    NccParseLocalDecl(false);
                }
                else
                {
                    NccParseExpr(MODE_RV);
                }
            }
            if (g_tok.kind != TK_SEMI)
            {
                NccError("expected ';' in for");
                return;
            }
            NccAdvance();
            uint32_t l_cond = NccNewLabel();
            uint32_t l_inc = NccNewLabel();
            uint32_t l_body = NccNewLabel();
            uint32_t l_end = NccNewLabel();
            NccDefLabel(l_cond);
            if (g_tok.kind != TK_SEMI)
            {
                NccParseExpr(MODE_RV);
                NccEmitTestRax();
                NccJz(l_end);
            }
            if (g_tok.kind != TK_SEMI)
            {
                NccError("expected ';' in for");
                return;
            }
            NccAdvance();
            NccJmp(l_body);
            if (g_loop_depth < NCC_MAX_LOOP)
            {
                g_loop_break[g_loop_depth] = (int)l_end;
                g_loop_cont[g_loop_depth] = (int)l_inc;
                g_loop_depth++;
            }
            NccDefLabel(l_inc);
            if (g_tok.kind != TK_RPAREN)
            {
                NccParseExpr(MODE_RV);
            }
            if (g_tok.kind != TK_RPAREN)
            {
                NccError("expected ')' in for");
                return;
            }
            NccAdvance();
            NccJmp(l_cond);
            NccDefLabel(l_body);
            if (g_tok.kind != TK_LBRACE)
            {
                NccError("expected '{' after for header");
                return;
            }
            NccParseStmt();
            if (g_loop_depth > 0)
            {
                g_loop_depth--;
            }
            NccJmp(l_inc);
            NccDefLabel(l_end);
            break;
        }
        case TK_RETURN:
        {
            NccAdvance();
            if (g_tok.kind != TK_SEMI)
            {
                NccParseExpr(MODE_RV);
            }
            if (g_tok.kind != TK_SEMI)
            {
                NccError("expected ';' after return");
                return;
            }
            NccAdvance();
            NccEmitEpilogue();
            break;
        }
        case TK_BREAK:
            NccAdvance();
            if (g_loop_depth == 0)
            {
                NccError("break outside of loop");
            }
            else
            {
                NccJmp((uint32_t)g_loop_break[g_loop_depth - 1U]);
            }
            if (g_tok.kind == TK_SEMI)
            {
                NccAdvance();
            }
            break;
        case TK_CONTINUE:
            NccAdvance();
            if (g_loop_depth == 0)
            {
                NccError("continue outside of loop");
            }
            else
            {
                NccJmp((uint32_t)g_loop_cont[g_loop_depth - 1U]);
            }
            if (g_tok.kind == TK_SEMI)
            {
                NccAdvance();
            }
            break;
        case TK_TYPEDEF:
            NccParseTypedef();
            break;
        case TK_ENUM:
            NccParseDeclSpec(&(NccDeclSpec){ 0 });
            if (g_tok.kind == TK_SEMI)
            {
                NccAdvance();
            }
            break;
        default:
            if (NccTokIsTypeStartTok(&g_tok))
            {
                NccParseLocalDecl(true);
            }
            else
            {
                NccParseExpr(MODE_RV);
                if (g_tok.kind == TK_SEMI)
                {
                    NccAdvance();
                }
            }
            break;
    }
    if (g_estack != 0)
    {
        NccError("expression stack imbalance");
        g_estack = 0;
    }
}

/* ------------------------------------------------------------------ */
/* functions                                                           */
/* ------------------------------------------------------------------ */

static void NccParseFunction(NccDeclSpec *ds, NccType *ret, const char *name)
{
    (void)ds;
    int fi = NccFindFunc(name);
    NccFunc *f;
    if (fi < 0)
    {
        if (g_func_count >= NCC_MAX_FUNCS)
        {
            NccError("too many functions");
            return;
        }
        f = &g_funcs[g_func_count++];
        strncpy(f->name, name, sizeof(f->name) - 1U);
        f->name[sizeof(f->name) - 1U] = '\0';
        f->ret = ret;
        f->nparams = 0;
        f->defined = false;
        f->has_proto = true;
        f->text_rel = 0;
    }
    else
    {
        f = &g_funcs[fi];
        if (f->defined)
        {
            NccError("duplicate function definition");
        }
        f->ret = ret;
        f->has_proto = true;
    }
    NccScopePush();
    uint32_t n = 0;
    if (g_tok.kind != TK_RPAREN)
    {
        if (g_tok.kind == TK_VOID && g_peek.kind == TK_RPAREN)
        {
            NccAdvance();
            NccAdvance();
        }
        else
        {
            for (;;)
            {
                if (g_tok.kind == TK_DOT)
                {
                    /* variadic arguments: consume the '...' and end the list */
                    while (g_tok.kind == TK_DOT)
                    {
                        NccAdvance();
                    }
                    break;
                }
                if (n >= NCC_MAX_PARAMS)
                {
                    NccError("too many parameters (max 16)");
                    break;
                }
                NccDeclSpec pds;
                NccParseDeclSpec(&pds);
                if (pds.type == NULL)
                {
                    pds.type = &g_ty_int;
                }
                char pname[64];
                NccType *pt = NccParseDeclarator(pds.type, pname, false);
                if (g_tok.kind == TK_LBRACK)
                {
                    uint32_t depth = 0;
                    do
                    {
                        if (g_tok.kind == TK_LBRACK)
                        {
                            depth++;
                        }
                        else if (g_tok.kind == TK_RBRACK)
                        {
                            depth--;
                        }
                        if (depth > 0)
                        {
                            NccAdvance();
                        }
                    } while (depth > 0 && g_tok.kind != TK_EOF);
                    if (g_tok.kind == TK_RBRACK)
                    {
                        NccAdvance();
                    }
                    pt = NccPtrType(pt->elem != NULL ? pt->elem : pt);
                }
                if (pt->kind == TY_ARR)
                {
                    pt = NccPtrType(pt->elem);
                }
                if (pname[0] != '\0')
                {
                    if (g_locals_count < NCC_MAX_LOCALS)
                    {
                        NccVar *v = &g_locals[g_locals_count++];
                        strncpy(v->name, pname, sizeof(v->name) - 1U);
                        v->name[sizeof(v->name) - 1U] = '\0';
                        v->type = pt;
                        v->kind = VAR_PARAM;
                        v->offset = -8 - 8 * (int)n;
                        v->addr = 0;
                    }
                }
                f->params[n] = pt;
                n++;
                if (g_tok.kind == TK_COMMA)
                {
                    NccAdvance();
                    continue;
                }
                break;
            }
            if (g_tok.kind == TK_RPAREN)
            {
                NccAdvance();
            }
        }
    }
    else
    {
        NccAdvance();
    }
    f->nparams = n;

    if (g_tok.kind != TK_LBRACE)
    {
        if (g_tok.kind == TK_SEMI)
        {
            NccAdvance();
        }
        NccScopePop();
        return;
    }
    NccAdvance();

    f->text_rel = g_text_size;
    if (g_first_func_rel == UINT32_MAX)
    {
        g_first_func_rel = g_text_size;
    }
    g_cur_func = f;

    NccTextByte(0x55);
    NccTextByte(0x48); NccTextByte(0x89); NccTextByte(0xE5);
    NccTextByte(0x48); NccTextByte(0x81); NccTextByte(0xEC);
    g_frame_patch_pos = (int)g_text_size;
    NccTextU32(0);

    g_local_save_off = g_local_off;
    g_local_save_min = g_local_min_off;
    g_local_off = -8 * (int)f->nparams;
    g_local_min_off = g_local_off;

    for (uint32_t i = 0; i < f->nparams; i++)
    {
        if (g_locals_count - (f->nparams - i) >= 0)
        {
            NccVar *v = &g_locals[g_locals_count - f->nparams + i];
            if (v->kind == VAR_PARAM)
            {
                NccEmitArgStore(NccArgReg(i), v->type, v->offset);
            }
        }
    }

    NccScopePush();
    while (g_tok.kind != TK_RBRACE && g_tok.kind != TK_EOF)
    {
        NccParseStmt();
    }
    if (g_tok.kind == TK_RBRACE)
    {
        NccAdvance();
    }
    NccScopePop();

    uint32_t frame = (uint32_t)(-g_local_min_off);
    frame = (frame + 15U) & ~15U;
    NccPatchU32((uint32_t)g_frame_patch_pos, frame);
    NccEmitEpilogue();
    f->defined = true;
    g_cur_func = NULL;
    g_local_off = g_local_save_off;
    g_local_min_off = g_local_save_min;
}

/* ------------------------------------------------------------------ */
/* top-level, finalize, ELF output                                     */
/* ------------------------------------------------------------------ */

static uint64_t NccResolveName(const char *name)
{
    for (uint32_t i = 0; i < g_func_count; i++)
    {
        if (strcmp(g_funcs[i].name, name) == 0 && g_funcs[i].defined)
        {
            return NCC_BASE + NCC_HDRSZ + g_data_size + g_funcs[i].text_rel;
        }
    }
    for (uint32_t i = 0; i < g_globals_count; i++)
    {
        if (strcmp(g_globals[i].name, name) == 0)
        {
            return g_globals[i].addr;
        }
    }
    for (uint32_t i = 0; i < NccSymbolCount; i++)
    {
        if (strcmp(NccSymbols[i].Name, name) == 0)
        {
            return NccSymbols[i].Addr;
        }
    }
    for (uint32_t i = 0; i < g_func_count; i++)
    {
        if (strcmp(g_funcs[i].name, name) == 0)
        {
            return UINT64_MAX;
        }
    }
    return 0;
}

static bool NccFinalize(bool ResolveExternals)
{
    bool ok = true;
    uint32_t i;
    if (ResolveExternals)
    {
        for (i = 0; i < g_ref_count; i++)
        {
            uint64_t addr = NccResolveName(g_refs[i].name);
            if (addr == 0)
            {
                char buf[96];
                strncpy(buf, "undefined symbol '", sizeof(buf) - 1U);
                strncpy(buf + strlen(buf), g_refs[i].name,
                        sizeof(buf) - strlen(buf) - 1U);
                strncpy(buf + strlen(buf), "'",
                        sizeof(buf) - strlen(buf) - 1U);
                NccErrorAt(buf, 1);
                ok = false;
                continue;
            }
            if (addr == UINT64_MAX)
            {
                char buf[96];
                strncpy(buf, "function declared but never defined: ",
                        sizeof(buf) - 1U);
                strncpy(buf + strlen(buf), g_refs[i].name,
                        sizeof(buf) - strlen(buf) - 1U);
                NccErrorAt(buf, 1);
                ok = false;
                continue;
            }
            NccPatchU64(g_refs[i].pos, addr);
        }
    }
    for (i = 0; i < g_jump_count; i++)
    {
        uint32_t target = 0;
        bool found = false;
        for (uint32_t j = 0; j < g_label_count; j++)
        {
            if (g_labels[j].label == g_jumps[i].label)
            {
                target = g_labels[j].pos;
                found = true;
                break;
            }
        }
        if (!found)
        {
            NccError("internal: unresolved jump");
            ok = false;
            continue;
        }
        int32_t rel = (int32_t)target - (int32_t)(g_jumps[i].pos + 4U);
        NccPatchU32(g_jumps[i].pos, (uint32_t)rel);
    }
    return ok;
}

static void NccStore16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static void NccStore32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static void NccStore64(uint8_t *p, uint64_t v)
{
    for (uint32_t i = 0; i < 8; i++)
    {
        p[i] = (uint8_t)((v >> (i * 8U)) & 0xFFU);
    }
}

static bool NccBuildElf(uint8_t **OutImage, uint32_t *OutSize)
{
    uint64_t text_base_va = NCC_BASE + NCC_HDRSZ + g_data_size;
    uint64_t entry;
    int mi = NccFindFunc("main");
    if (mi >= 0 && g_funcs[mi].defined)
    {
        entry = text_base_va + g_funcs[mi].text_rel;
    }
    else
    {
        entry = text_base_va + g_first_func_rel;
    }
    uint32_t image_size = NCC_HDRSZ + g_data_size + g_text_size;
    uint8_t *img = (uint8_t *)kmalloc(image_size);
    memset(img, 0, image_size);

    img[0] = 0x7F; img[1] = 'E'; img[2] = 'L'; img[3] = 'F';
    img[4] = 2;
    img[5] = 1;
    img[6] = 1;
    img[7] = 0;
    NccStore16(img + 16, 2);
    NccStore16(img + 18, 62);
    NccStore32(img + 20, 1);
    NccStore64(img + 24, entry);
    NccStore64(img + 32, 64);
    NccStore64(img + 40, 0);
    NccStore32(img + 48, 0);
    NccStore16(img + 52, 64);
    NccStore16(img + 54, 56);
    NccStore16(img + 56, 1);
    NccStore16(img + 58, 0);
    NccStore16(img + 60, 0);
    NccStore16(img + 62, 0);

    uint8_t *p = img + 64;
    NccStore32(p + 0, 1);
    NccStore32(p + 4, 7);
    NccStore64(p + 8, NCC_HDRSZ);
    NccStore64(p + 16, NCC_BASE + NCC_HDRSZ);
    NccStore64(p + 24, NCC_BASE + NCC_HDRSZ);
    NccStore64(p + 32, (uint64_t)g_data_size + g_text_size);
    NccStore64(p + 40, (uint64_t)g_data_size + g_text_size);
    NccStore64(p + 48, 0x1000);

    memcpy(img + NCC_HDRSZ, g_data, g_data_size);
    memcpy(img + NCC_HDRSZ + g_data_size, g_text, g_text_size);

    *OutImage = img;
    *OutSize = image_size;
    return true;
}

static bool NccParseTopLevelItem(void)
{
    if (g_tok.kind == TK_EOF)
    {
        return false;
    }
    switch (g_tok.kind)
    {
        case TK_TYPEDEF:
            NccParseTypedef();
            return true;
        case TK_ENUM:
            NccParseDeclSpec(&(NccDeclSpec){ 0 });
            if (g_tok.kind == TK_SEMI)
            {
                NccAdvance();
            }
            return true;
        case TK_SEMI:
            NccAdvance();
            return true;
        default:
            break;
    }
    NccDeclSpec ds;
    NccParseDeclSpec(&ds);
    if (ds.type == NULL)
    {
        NccError("expected a declaration");
        while (g_tok.kind != TK_SEMI && g_tok.kind != TK_EOF)
        {
            NccAdvance();
        }
        if (g_tok.kind == TK_SEMI)
        {
            NccAdvance();
        }
        return true;
    }
    for (;;)
    {
        char name[64];
        NccType *t = NccParseDeclarator(ds.type, name, true);
        if (g_tok.kind == TK_LPAREN)
        {
            if (name[0] == '\0')
            {
                NccError("function declarator without a name");
                return true;
            }
            NccAdvance();
            NccParseFunction(&ds, t, name);
            return true;
        }
        if (name[0] == '\0')
        {
            NccError("expected identifier in declaration");
            if (g_tok.kind == TK_SEMI)
            {
                NccAdvance();
            }
            return true;
        }
        NccParseGlobalDecl(&ds, name, t);
        if (g_tok.kind == TK_COMMA)
        {
            NccAdvance();
            continue;
        }
        break;
    }
    if (g_tok.kind != TK_SEMI)
    {
        NccError("expected ';' after declaration");
    }
    else
    {
        NccAdvance();
    }
    return true;
}

static bool NccCompileFrontEnd(const char *SourcePath, const char *Source)
{
    g_file = SourcePath;
    g_err_count = 0;
    NccResetParser();
    g_macro_count = 0;
    NccMacroDefine("__NCC__", "1");
    g_cond_depth = 0;
    g_pp_ok = true;
    g_last_expr_is_addr = false;
    g_last_is_func = false;

    NccBuf pp;
    NccBufInit(&pp, 4096);
    NccPpProcess(SourcePath, Source, &pp, 0);
    if (!g_pp_ok)
    {
        return false;
    }
    NccBufPut(&pp, 0);

    g_lex_p = (const char *)pp.data;
    g_lex_line = 1;
    g_lex_line_start = true;
    g_peek_valid = false;
    NccAdvance();

    while (g_tok.kind != TK_EOF)
    {
        NccParseTopLevelItem();
        if (g_err_count > 0)
        {
            return false;
        }
    }
    if (g_first_func_rel == UINT32_MAX)
    {
        NccError("no functions found in source");
        return false;
    }
    return true;
}

bool NccCompile(const char *SourcePath, const char *Source,
                uint32_t SourceSize, uint8_t **OutImage, uint32_t *OutSize)
{
    (void)SourceSize;
    *OutImage = NULL;
    *OutSize = 0;
    if (!NccCompileFrontEnd(SourcePath, Source))
    {
        return false;
    }
    if (!NccFinalize(true))
    {
        return false;
    }
    return NccBuildElf(OutImage, OutSize);
}

/* ------------------------------------------------------------------ */
/* relocatable object output (N64O) for mixed C/assembly linking       */
/* ------------------------------------------------------------------ */

#include "n64a/n64a.h"

/* String literals are named "$N" which is unique only within one
 * compilation; prefix them with the source file so two objects cannot
 * collide in the flat linker namespace. */
static void NccMapSymbolName(const char *src, char *dst)
{
    const char *base = g_file;
    const char *b;
    uint32_t i = 0;

    for (b = g_file; *b != '\0'; b++)
    {
        if (*b == '/')
        {
            base = b + 1;
        }
    }

    if (src[0] != '$')
    {
        strncpy(dst, src, N64O_NAME_MAX - 1U);
        dst[N64O_NAME_MAX - 1U] = '\0';
        return;
    }
    dst[i++] = '$';
    while (base != NULL && *base != '\0' && i < 40U &&
           *base != '.')
    {
        char c = *base++;
        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c - 'a' + 'A');
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
        {
            c = '_';
        }
        dst[i++] = c;
    }
    dst[i++] = '$';
    while (src != NULL && *src != '\0' && i < N64O_NAME_MAX - 1U)
    {
        dst[i++] = *src++;
    }
    dst[i] = '\0';
}

static bool NccBuildObject(const char *SourcePath,
                           uint8_t **OutObject, uint32_t *OutObjectSize)
{
    bool has_main = false;
    uint32_t nsyms = 0;
    uint32_t nrels = 0;
    uint32_t total;
    uint32_t off;
    uint8_t *img;
    uint32_t i;

    for (i = 0; i < g_func_count; i++)
    {
        if (g_funcs[i].defined)
        {
            nsyms++;
        }
    }
    for (i = 0; i < g_globals_count; i++)
    {
        if (g_globals[i].kind == VAR_GLOBAL)
        {
            nsyms++;
        }
    }
    nrels = g_ref_count;

    total = N64O_HDRSZ + g_text_size + g_data_size +
            nsyms * (uint32_t)sizeof(N64ObjSymbol) +
            nrels * (uint32_t)sizeof(N64ObjReloc);
    img = (uint8_t *)kmalloc(total);
    if (img == NULL)
    {
        NccError("out of memory building object");
        return false;
    }
    memset(img, 0, total);

    memcpy(img, N64O_MAGIC, 7);
    img[7] = '\0';
    NccStore16(img + 8, (uint16_t)N64O_VERSION);
    NccStore16(img + 10, (uint16_t)N64O_HDRSZ);
    NccStore32(img + 12, has_main ? 1U : 0U);
    NccStore32(img + 16, g_text_size);
    NccStore32(img + 20, g_data_size);
    NccStore32(img + 24, nsyms);
    NccStore32(img + 28, nrels);

    off = N64O_HDRSZ;
    if (g_text_size != 0U)
    {
        memcpy(img + off, g_text, g_text_size);
    }
    off += g_text_size;
    if (g_data_size != 0U)
    {
        memcpy(img + off, g_data, g_data_size);
    }
    off += g_data_size;

    /* symbols: defined functions first, then real globals */
    for (i = 0; i < g_func_count; i++)
    {
        N64ObjSymbol s;
        if (!g_funcs[i].defined)
        {
            continue;
        }
        memset(&s, 0, sizeof(s));
        if (strcmp(g_funcs[i].name, "main") == 0)
        {
            has_main = true;
        }
        NccMapSymbolName(g_funcs[i].name, s.Name);
        s.Value = g_funcs[i].text_rel;
        s.Kind = N64O_SYM_FUNC;
        s.Section = N64_SEC_TEXT;
        memcpy(img + off, &s, sizeof(s));
        off += (uint32_t)sizeof(s);
    }
    for (i = 0; i < g_globals_count; i++)
    {
        N64ObjSymbol s;
        if (g_globals[i].kind != VAR_GLOBAL)
        {
            continue;
        }
        memset(&s, 0, sizeof(s));
        NccMapSymbolName(g_globals[i].name, s.Name);
        s.Value = (uint32_t)(g_globals[i].addr -
                             (NCC_BASE + NCC_HDRSZ));
        s.Kind = N64O_SYM_DATA;
        s.Section = N64_SEC_DATA;
        memcpy(img + off, &s, sizeof(s));
        off += (uint32_t)sizeof(s);
    }

    /* every symbolic reference becomes an ABS64 relocation */
    for (i = 0; i < g_ref_count; i++)
    {
        N64ObjReloc r;
        memset(&r, 0, sizeof(r));
        NccMapSymbolName(g_refs[i].name, r.Name);
        r.Offset = g_refs[i].pos;
        r.Type = N64O_R_ABS64;
        r.Section = N64_SEC_TEXT;
        r.Addend = 0;
        memcpy(img + off, &r, sizeof(r));
        off += (uint32_t)sizeof(r);
    }

    NccStore32(img + 12, has_main ? 1U : 0U);

    (void)SourcePath;
    *OutObject = img;
    *OutObjectSize = total;
    return true;
}

bool NccCompileObject(const char *SourcePath, const char *Source,
                      uint32_t SourceSize, uint8_t **OutObject,
                      uint32_t *OutObjectSize)
{
    (void)SourceSize;
    *OutObject = NULL;
    *OutObjectSize = 0;
    if (!NccCompileFrontEnd(SourcePath, Source))
    {
        return false;
    }
    /* internal rel32 jumps are self-contained; external references stay
     * unpatched and become relocations. */
    if (!NccFinalize(false))
    {
        return false;
    }
    return NccBuildObject(SourcePath, OutObject, OutObjectSize);
}
