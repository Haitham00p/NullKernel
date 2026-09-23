#ifndef N64A_PARSE_H
#define N64A_PARSE_H

#include "n64a_asm.h"
#include "n64a_lex.h"

/* Parses the full token stream and fills the assembler state.
 * Returns false on any diagnostic failure. */
bool N64aParse(N64aAsm *a, N64Lex *lx);

#endif
