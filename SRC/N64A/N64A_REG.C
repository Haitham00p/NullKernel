/* n64a — x86-64 general purpose register name table */
#include "n64a_reg.h"

typedef struct
{
    const char *Name;
    uint8_t     Index;
    uint8_t     Size;
    uint8_t     NeedsRex;
} N64RegEntry;

static const N64RegEntry N64RegTable[] =
{
    /* 64 bit */
    { "rax", 0,  N64_SZ64, 0 }, { "rcx", 1,  N64_SZ64, 0 },
    { "rdx", 2,  N64_SZ64, 0 }, { "rbx", 3,  N64_SZ64, 0 },
    { "rsp", 4,  N64_SZ64, 0 }, { "rbp", 5,  N64_SZ64, 0 },
    { "rsi", 6,  N64_SZ64, 0 }, { "rdi", 7,  N64_SZ64, 0 },
    { "r8",  8,  N64_SZ64, 1 }, { "r9",  9,  N64_SZ64, 1 },
    { "r10", 10, N64_SZ64, 1 }, { "r11", 11, N64_SZ64, 1 },
    { "r12", 12, N64_SZ64, 1 }, { "r13", 13, N64_SZ64, 1 },
    { "r14", 14, N64_SZ64, 1 }, { "r15", 15, N64_SZ64, 1 },

    /* 32 bit */
    { "eax", 0,  N64_SZ32, 0 }, { "ecx", 1,  N64_SZ32, 0 },
    { "edx", 2,  N64_SZ32, 0 }, { "ebx", 3,  N64_SZ32, 0 },
    { "esp", 4,  N64_SZ32, 0 }, { "ebp", 5,  N64_SZ32, 0 },
    { "esi", 6,  N64_SZ32, 0 }, { "edi", 7,  N64_SZ32, 0 },
    { "r8d", 8,  N64_SZ32, 1 }, { "r9d", 9,  N64_SZ32, 1 },
    { "r10d", 10, N64_SZ32, 1 }, { "r11d", 11, N64_SZ32, 1 },
    { "r12d", 12, N64_SZ32, 1 }, { "r13d", 13, N64_SZ32, 1 },
    { "r14d", 14, N64_SZ32, 1 }, { "r15d", 15, N64_SZ32, 1 },

    /* 16 bit */
    { "ax", 0,  N64_SZ16, 0 }, { "cx", 1,  N64_SZ16, 0 },
    { "dx", 2,  N64_SZ16, 0 }, { "bx", 3,  N64_SZ16, 0 },
    { "sp", 4,  N64_SZ16, 0 }, { "bp", 5,  N64_SZ16, 0 },
    { "si", 6,  N64_SZ16, 0 }, { "di", 7,  N64_SZ16, 0 },
    { "r8w", 8,  N64_SZ16, 1 }, { "r9w", 9,  N64_SZ16, 1 },
    { "r10w", 10, N64_SZ16, 1 }, { "r11w", 11, N64_SZ16, 1 },
    { "r12w", 12, N64_SZ16, 1 }, { "r13w", 13, N64_SZ16, 1 },
    { "r14w", 14, N64_SZ16, 1 }, { "r15w", 15, N64_SZ16, 1 },

    /* 8 bit (legacy names need no REX; spl/bpl/sil/dil and r8b.. need it) */
    { "al", 0,  N64_SZ8, 0 }, { "cl", 1,  N64_SZ8, 0 },
    { "dl", 2,  N64_SZ8, 0 }, { "bl", 3,  N64_SZ8, 0 },
    { "ah", 4,  N64_SZ8, 0 }, { "ch", 5,  N64_SZ8, 0 },
    { "dh", 6,  N64_SZ8, 0 }, { "bh", 7,  N64_SZ8, 0 },
    { "spl", 4, N64_SZ8, 1 }, { "bpl", 5, N64_SZ8, 1 },
    { "sil", 6, N64_SZ8, 1 }, { "dil", 7, N64_SZ8, 1 },
    { "r8b", 8,  N64_SZ8, 1 }, { "r9b", 9,  N64_SZ8, 1 },
    { "r10b", 10, N64_SZ8, 1 }, { "r11b", 11, N64_SZ8, 1 },
    { "r12b", 12, N64_SZ8, 1 }, { "r13b", 13, N64_SZ8, 1 },
    { "r14b", 14, N64_SZ8, 1 }, { "r15b", 15, N64_SZ8, 1 }
};

#define N64_REG_TABLE_LEN \
    (unsigned)(sizeof(N64RegTable) / sizeof(N64RegTable[0]))

bool N64RegFind(const char *Name, N64Reg *Out)
{
    unsigned i;
    for (i = 0; i < N64_REG_TABLE_LEN; i++)
    {
        if (N64aStrCmp(N64RegTable[i].Name, Name) == 0)
        {
            Out->Index = N64RegTable[i].Index;
            Out->Size = N64RegTable[i].Size;
            Out->NeedsRex = N64RegTable[i].NeedsRex != 0U;
            return true;
        }
    }
    return false;
}
