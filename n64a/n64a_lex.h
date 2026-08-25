#ifndef N64A_LEX_H
#define N64A_LEX_H

#include "n64a.h"

typedef enum
{
    N64_T_EOF = 0,
    N64_T_EOL,
    N64_T_IDENT,      /* label / mnemonic / directive / register name   */
    N64_T_NUMBER,     /* integer literal                                */
    N64_T_STRING,     /* "..."                                          */
    N64_T_CHAR,       /* 'c'                                            */
    N64_T_COMMA,
    N64_T_COLON,
    N64_T_LBRACK,
    N64_T_RBRACK,
    N64_T_PLUS,
    N64_T_MINUS,
    N64_T_STAR,
    N64_T_LPAREN,
    N64_T_RPAREN
} N64TokKind;

typedef struct
{
    N64TokKind      Kind;
    uint64_t        Num;      /* N64_T_NUMBER / N64_T_CHAR             */
    char            Text[64]; /* N64_T_IDENT                            */
    const char     *Str;      /* N64_T_STRING: pointer into line buffer */
    uint32_t        StrLen;
    uint32_t        Line;
} N64Tok;

typedef struct
{
    const char *File;
    const char *Src;
    uint64_t    Len;
    uint64_t    Pos;
    uint32_t    Line;

    /* current + one token of lookahead (line-oriented grammar needs it) */
    N64Tok      Cur;
    bool        CurValid;
    N64Tok      Peek;
    bool        PeekValid;

    N64aDiag   *Diag;         /* shared diagnostics                     */
} N64Lex;

void N64LexInit(N64Lex *lx, const char *file, const char *src,
                unsigned long long len, N64aDiag *diag);
bool N64LexNext(N64Lex *lx, N64Tok *out);
bool N64LexAdvance(N64Lex *lx);           /* Cur <- Peek <- next          */
bool N64LexPeekIs(N64Lex *lx, N64TokKind k);

#endif
