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

/* n64a — x86-64 instruction encoder.
 *
 * Implements real REX / ModR/M / SIB / displacement / immediate encoding
 * for the supported subset of x86-64.  No pre-assembled blobs: every byte
 * is derived from the operand structure.
 */
#include "n64a_enc.h"

/* ------------------------------------------------------------------ */
/* buffer                                                              */
/* ------------------------------------------------------------------ */

void N64BufInit(N64Buf *b)
{
    b->Data = NULL;
    b->Size = 0;
    b->Cap = 0;
}

bool N64BufReserve(N64Buf *b, uint32_t extra)
{
    uint32_t nc;
    uint8_t *nd;

    if (b->Size + extra <= b->Cap)
    {
        return true;
    }
    nc = b->Cap == 0U ? 256U : b->Cap;
    while (nc < b->Size + extra)
    {
        if (nc > 0x40000000U)
        {
            return false;
        }
        nc *= 2U;
    }
    nd = (uint8_t *)N64A_MALLOC(nc);
    if (nd == NULL)
    {
        return false;
    }
    if (b->Data != NULL)
    {
        N64aMemCpy(nd, b->Data, b->Size);
        N64A_FREE(b->Data);
    }
    b->Data = nd;
    b->Cap = nc;
    return true;
}

void N64BufByte(N64Buf *b, uint8_t v)
{
    if (N64BufReserve(b, 1))
    {
        b->Data[b->Size++] = v;
    }
}

void N64BufBytes(N64Buf *b, const void *src, uint32_t n)
{
    if (N64BufReserve(b, n))
    {
        N64aMemCpy(&b->Data[b->Size], src, n);
        b->Size += n;
    }
}

void N64BufU16(N64Buf *b, uint16_t v)
{
    N64BufByte(b, (uint8_t)(v & 0xFFU));
    N64BufByte(b, (uint8_t)((v >> 8) & 0xFFU));
}

void N64BufU32(N64Buf *b, uint32_t v)
{
    N64BufByte(b, (uint8_t)(v & 0xFFU));
    N64BufByte(b, (uint8_t)((v >> 8) & 0xFFU));
    N64BufByte(b, (uint8_t)((v >> 16) & 0xFFU));
    N64BufByte(b, (uint8_t)((v >> 24) & 0xFFU));
}

void N64BufU64(N64Buf *b, uint64_t v)
{
    unsigned i;
    for (i = 0; i < 8; i++)
    {
        N64BufByte(b, (uint8_t)((v >> (i * 8U)) & 0xFFU));
    }
}

void N64BufFree(N64Buf *b)
{
    if (b->Data != NULL)
    {
        N64A_FREE(b->Data);
        b->Data = NULL;
    }
    b->Size = 0;
    b->Cap = 0;
}

/* ------------------------------------------------------------------ */
/* condition codes                                                     */
/* ------------------------------------------------------------------ */

typedef struct
{
    const char *Name;
    uint8_t     Cc;
} N64CondEntry;

static const N64CondEntry N64CondTable[] =
{
    { "o",  N64_CC_O },  { "no", N64_CC_NO },
    { "b",  N64_CC_B },  { "c",  N64_CC_B },  { "nae", N64_CC_B },
    { "ae", N64_CC_AE }, { "nb", N64_CC_AE }, { "nc",  N64_CC_AE },
    { "e",  N64_CC_E },  { "z",  N64_CC_E },
    { "ne", N64_CC_NE }, { "nz", N64_CC_NE },
    { "be", N64_CC_BE }, { "na", N64_CC_BE },
    { "a",  N64_CC_A },  { "nbe", N64_CC_A },
    { "s",  N64_CC_S },  { "ns", N64_CC_NS },
    { "p",  N64_CC_P },  { "pe", N64_CC_P },
    { "np", N64_CC_NP }, { "po", N64_CC_NP },
    { "l",  N64_CC_L },  { "nge", N64_CC_L },
    { "ge", N64_CC_GE }, { "nl", N64_CC_GE },
    { "le", N64_CC_LE }, { "ng", N64_CC_LE },
    { "g",  N64_CC_G },  { "nle", N64_CC_G }
};

#define N64_COND_LEN (unsigned)(sizeof(N64CondTable) / sizeof(N64CondTable[0]))

bool N64CondFind(const char *CcName, uint8_t *Out)
{
    unsigned i;
    for (i = 0; i < N64_COND_LEN; i++)
    {
        if (N64aStrCmp(N64CondTable[i].Name, CcName) == 0)
        {
            if (Out != NULL)
            {
                *Out = N64CondTable[i].Cc;
            }
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* prefixes                                                            */
/* ------------------------------------------------------------------ */

void N64EncRex(N64Buf *b, unsigned flags, bool force)
{
    if (flags != 0U || force)
    {
        N64BufByte(b, (uint8_t)(0x40U | flags));
    }
}

unsigned N64EncMemRexBits(const N64MemOp *m, unsigned extra)
{
    unsigned rex = extra;
    if (m->HasBase && m->BaseIdx >= 8)   rex |= N64_REX_B;
    if (m->HasIndex && m->IndexIdx >= 8) rex |= N64_REX_X;
    return rex;
}

/* ------------------------------------------------------------------ */
/* modrm / sib                                                         */
/* ------------------------------------------------------------------ */

void N64EncModRmRegReg(N64Buf *b, uint8_t reg_field, const N64Reg *rm)
{
    N64BufByte(b, (uint8_t)(0xC0U | ((reg_field & 7U) << 3) |
                            (rm->Index & 7U)));
}

/* Emits the displacement that follows a mod=1/mod=2 or absolute form.
 * Symbolic displacements become DISP32 relocations.                   */
static void EmitDisp32(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                       const N64MemOp *m)
{
    if (m->Sym[0] != '\0')
    {
        sink->Add(sink->User, reloc_base + b->Size,
                  N64O_R_DISP32, m->Sym, m->SymAddend);
    }
    N64BufU32(b, (uint32_t)(int32_t)m->Disp);
}

/* True when the displacement fits in a sign-extended imm8. */
static bool DispIs8(const N64MemOp *m)
{
    return m->Sym[0] == '\0' && m->Disp >= -128 && m->Disp <= 127;
}

void N64EncModRmMem(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                    const N64MemOp *m, uint8_t reg_field)
{
    bool sib_needed;
    bool has_index_real;

    /* --- absolute [disp32]: no base, no index -> rm = 101 ------------- */
    if (!m->HasBase && !m->HasIndex)
    {
        N64BufByte(b, (uint8_t)((reg_field << 3) | 0x05U));
        EmitDisp32(b, sink, reloc_base, m);
        return;
    }

    /* A SIB byte is required whenever an index register participates
     * (any scale, including 1) or when the base is rsp/r12.           */
    has_index_real = m->HasIndex;
    sib_needed = has_index_real ||
                 (m->HasBase && m->BaseIdx == 4U);

    if (sib_needed)
    {
        uint8_t base_bits = m->HasBase ? (uint8_t)(m->BaseIdx & 7U) : 5U;
        uint8_t idx_bits  = m->HasIndex ? (uint8_t)(m->IndexIdx & 7U) : 4U;
        uint8_t ss;
        uint8_t mod;

        switch (m->Scale)
        {
            case 2:  ss = 1; break;
            case 4:  ss = 2; break;
            case 8:  ss = 3; break;
            default: ss = 0; break;
        }

        if (!m->HasBase)
        {
            /* [index*scale + disp32]; base=101 in SIB means disp32 after */
            N64BufByte(b, (uint8_t)((reg_field << 3) | 0x04U));
            N64BufByte(b, (uint8_t)((ss << 6) | (idx_bits << 3) | 0x05U));
            EmitDisp32(b, sink, reloc_base, m);
            return;
        }

        if (m->Disp == 0 && m->Sym[0] == '\0' && m->BaseIdx != 5U)
        {
            mod = 0;
        }
        else if (DispIs8(m))
        {
            mod = 1;
        }
        else
        {
            mod = 2;
        }

        N64BufByte(b, (uint8_t)((mod << 6) | (reg_field << 3) | 0x04U));
        N64BufByte(b, (uint8_t)((ss << 6) | (idx_bits << 3) | base_bits));
        if (mod == 1)
        {
            N64BufByte(b, (uint8_t)(int8_t)m->Disp);
        }
        else if (mod == 2)
        {
            EmitDisp32(b, sink, reloc_base, m);
        }
        return;
    }

    /* --- base only ---------------------------------------------------- */
    {
        uint8_t rm = (uint8_t)(m->BaseIdx & 7U);

        if (m->Disp == 0 && m->Sym[0] == '\0' &&
            m->BaseIdx != 5U && m->BaseIdx != 13U)
        {
            N64BufByte(b, (uint8_t)((reg_field << 3) | rm)); /* mod=00 */
            return;
        }
        if (DispIs8(m))
        {
            N64BufByte(b, (uint8_t)(0x40U | (reg_field << 3) | rm));
            N64BufByte(b, (uint8_t)(int8_t)m->Disp);
            return;
        }
        N64BufByte(b, (uint8_t)(0x80U | (reg_field << 3) | rm));
        EmitDisp32(b, sink, reloc_base, m);
    }
}

/* ------------------------------------------------------------------ */
/* shared helpers                                                      */
/* ------------------------------------------------------------------ */

static unsigned RegRexR(const N64Reg *r)
{
    return r->Index >= 8 ? N64_REX_R : 0U;
}

static unsigned RegRexB(const N64Reg *r)
{
    return r->Index >= 8 ? N64_REX_B : 0U;
}

/* 16-bit operand size override prefix. */
static void N64EncOpSize16(N64Buf *b, unsigned size)
{
    if (size == N64_SZ16)
    {
        N64BufByte(b, 0x66);
    }
}

static void EmitImm(N64Buf *b, uint64_t imm, unsigned size)
{
    if (size == N64_SZ8)
    {
        N64BufByte(b, (uint8_t)imm);
    }
    else if (size == N64_SZ16)
    {
        N64BufU16(b, (uint16_t)imm);
    }
    else
    {
        N64BufU32(b, (uint32_t)imm);
    }
}

/* ------------------------------------------------------------------ */
/* arithmetic family                                                   */
/* op_base: ADD=00 OR=08 ADC=10 SBB=18 AND=20 SUB=28 XOR=30 CMP=38     */
/* Forms:                                                              */
/*   op r/m, r      opcode+0                                           */
/*   op r, r/m      opcode+2 (direction reversed)                      */
/*   op AL/…, imm   opcode+4                                           */
/*   op r/m, imm    80/81/83 group                                     */
/* ------------------------------------------------------------------ */

bool N64EncArithRegMem(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                       uint8_t op_base, const N64Operand *dst,
                       const N64Operand *src)
{
    unsigned rex;
    unsigned size;
    bool force;
    uint8_t op;

    if (dst->Kind == N64_OP_REG && src->Kind == N64_OP_REG)
    {
        size = dst->Reg.Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(&src->Reg) | RegRexB(&dst->Reg);
        force = size == N64_SZ8 && (dst->Reg.NeedsRex || src->Reg.NeedsRex);
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        op = (uint8_t)(size == N64_SZ8 ? op_base : (op_base + 1U));
        N64BufByte(b, op); /* r/m <- r */
        N64EncModRmRegReg(b, (uint8_t)(src->Reg.Index & 7U), &dst->Reg);
        return true;
    }

    if (dst->Kind == N64_OP_MEM && src->Kind == N64_OP_REG)
    {
        const N64MemOp *m = &dst->Mem;
        const N64Reg *r = &src->Reg;
        size = r->Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(r) | N64EncMemRexBits(m, 0U);
        force = size == N64_SZ8 && (r->NeedsRex || m->BaseIdx >= 8 ||
                                    m->IndexIdx >= 8);
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        op = (uint8_t)(size == N64_SZ8 ? op_base : (op_base + 1U));
        N64BufByte(b, op);
        N64EncModRmMem(b, sink, reloc_base, m, (uint8_t)(r->Index & 7U));
        return true;
    }

    if (dst->Kind == N64_OP_REG && src->Kind == N64_OP_MEM)
    {
        const N64MemOp *m = &src->Mem;
        const N64Reg *r = &dst->Reg;
        size = r->Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(r) | N64EncMemRexBits(m, 0U);
        force = size == N64_SZ8 && (r->NeedsRex || m->BaseIdx >= 8 ||
                                    m->IndexIdx >= 8);
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        op = (uint8_t)(size == N64_SZ8 ? (op_base + 2U) : (op_base + 3U));
        N64BufByte(b, op); /* reversed direction */
        N64EncModRmMem(b, sink, reloc_base, m, (uint8_t)(r->Index & 7U));
        return true;
    }

    return false;
}

/* ext_index: ADD=0 OR=1 ADC=2 SBB=3 AND=4 SUB=5 XOR=6 CMP=7           */
bool N64EncArithImm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                    uint8_t ext_index, const N64Operand *dst,
                    uint64_t imm, unsigned size, bool dst_is_acc)
{
    unsigned rex;
    bool force;

    if (dst->Kind == N64_OP_REG)
    {
        const N64Reg *r = &dst->Reg;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) | RegRexB(r);
        force = size == N64_SZ8 && r->NeedsRex;

        /* accumulator short forms: 04/05 style */
        if (dst_is_acc && r->Index == 0 && !force)
        {
            if (size == N64_SZ8)
            {
                N64BufByte(b, (uint8_t)((ext_index << 3) | 0x04U));
                EmitImm(b, imm, N64_SZ8);
                return true;
            }
            if (size == N64_SZ16)
            {
                N64EncOpSize16(b, size);
                N64BufByte(b, (uint8_t)((ext_index << 3) | 0x05U));
                EmitImm(b, imm, N64_SZ16);
                return true;
            }
            /* 32/64-bit accumulator: imm32 (sign extended in 64-bit) */
            N64EncRex(b, size == N64_SZ64 ? N64_REX_W : 0U, false);
            N64BufByte(b, (uint8_t)((ext_index << 3) | 0x05U));
            N64BufU32(b, (uint32_t)imm);
            return true;
        }

        /* sign-extended imm8 group form for 16/32/64-bit operands */
        if (size != N64_SZ8 &&
            (int64_t)imm >= -128 && (int64_t)imm <= 127)
        {
            N64EncOpSize16(b, size);
            N64EncRex(b, rex, false);
            N64BufByte(b, 0x83);
            N64EncModRmRegReg(b, ext_index, r);
            N64BufByte(b, (uint8_t)(int8_t)imm);
            return true;
        }

        if (size == N64_SZ8)
        {
            if ((int64_t)imm < -128 || (int64_t)imm > 255)
            {
                return false;
            }
            N64EncRex(b, rex, force);
            N64BufByte(b, 0x80);
            N64EncModRmRegReg(b, ext_index, r);
            N64BufByte(b, (uint8_t)imm);
            return true;
        }

        if (size == N64_SZ16)
        {
            N64EncOpSize16(b, size);
            N64EncRex(b, rex, false);
            N64BufByte(b, 0x81);
            N64EncModRmRegReg(b, ext_index, r);
            N64BufU16(b, (uint16_t)imm);
            return true;
        }

        /* 32-bit: imm32.  64-bit: imm32 sign-extended (81 /digit id). */
        N64EncRex(b, rex, false);
        N64BufByte(b, 0x81);
        N64EncModRmRegReg(b, ext_index, r);
        N64BufU32(b, (uint32_t)imm);
        return true;
    }

    if (dst->Kind == N64_OP_MEM)
    {
        const N64MemOp *m = &dst->Mem;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              N64EncMemRexBits(m, 0U);

        if (size != N64_SZ8 &&
            (int64_t)imm >= -128 && (int64_t)imm <= 127)
        {
            N64EncOpSize16(b, size);
            N64EncRex(b, rex, false);
            N64BufByte(b, 0x83);
            N64EncModRmMem(b, sink, reloc_base, m, ext_index);
            N64BufByte(b, (uint8_t)(int8_t)imm);
            return true;
        }
        if (size == N64_SZ8)
        {
            N64EncRex(b, rex, false);
            N64BufByte(b, 0x80);
            N64EncModRmMem(b, sink, reloc_base, m, ext_index);
            N64BufByte(b, (uint8_t)imm);
            return true;
        }
        if (size == N64_SZ16)
        {
            N64EncOpSize16(b, size);
            N64EncRex(b, rex, false);
            N64BufByte(b, 0x81);
            N64EncModRmMem(b, sink, reloc_base, m, ext_index);
            N64BufU16(b, (uint16_t)imm);
            return true;
        }
        N64EncRex(b, rex, false);
        N64BufByte(b, 0x81);
        N64EncModRmMem(b, sink, reloc_base, m, ext_index);
        N64BufU32(b, (uint32_t)imm);
        return true;
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* MOV                                                                 */
/* ------------------------------------------------------------------ */

bool N64EncMov(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
               const N64Operand *dst, const N64Operand *src)
{
    unsigned rex;
    unsigned size;
    bool force;

    if (dst->Kind == N64_OP_REG && src->Kind == N64_OP_REG)
    {
        size = dst->Reg.Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(&src->Reg) | RegRexB(&dst->Reg);
        force = size == N64_SZ8 &&
                (dst->Reg.NeedsRex || src->Reg.NeedsRex);
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, 0x89); /* mov r/m, r */
        N64EncModRmRegReg(b, (uint8_t)(src->Reg.Index & 7U), &dst->Reg);
        return true;
    }

    if (dst->Kind == N64_OP_MEM && src->Kind == N64_OP_REG)
    {
        const N64MemOp *m = &dst->Mem;
        const N64Reg *r = &src->Reg;
        size = r->Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(r) | N64EncMemRexBits(m, 0U);
        force = size == N64_SZ8 &&
                (r->NeedsRex || m->BaseIdx >= 8 || m->IndexIdx >= 8);
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, 0x89);
        N64EncModRmMem(b, sink, reloc_base, m, (uint8_t)(r->Index & 7U));
        return true;
    }

    if (dst->Kind == N64_OP_REG && src->Kind == N64_OP_MEM)
    {
        const N64MemOp *m = &src->Mem;
        const N64Reg *r = &dst->Reg;
        size = r->Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(r) | N64EncMemRexBits(m, 0U);
        force = size == N64_SZ8 &&
                (r->NeedsRex || m->BaseIdx >= 8 || m->IndexIdx >= 8);
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, 0x8B); /* mov r, r/m */
        N64EncModRmMem(b, sink, reloc_base, m, (uint8_t)(r->Index & 7U));
        return true;
    }

    if (dst->Kind == N64_OP_REG && src->Kind == N64_OP_IMM)
    {
        const N64Reg *r = &dst->Reg;
        uint64_t imm = src->Imm;

        /* symbolic immediates are handled by N64EncMovAbs64 */
        if (src->Sym[0] != '\0')
        {
            return false;
        }

        size = r->Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) | RegRexB(r);

        /* B8+r: imm encodes full operand width; for 64-bit only via
         * movabs (B8 imm64).  C7 /0 covers sign-extended imm32.        */
        if (size == N64_SZ64)
        {
            if ((int64_t)imm >= -2147483648LL && (int64_t)imm <= 2147483647LL)
            {
                N64EncRex(b, rex, false);
                N64BufByte(b, 0xC7);
                N64EncModRmRegReg(b, 0, r);
                N64BufU32(b, (uint32_t)imm);
                return true;
            }
            return false; /* caller falls back to movabs form */
        }

        if (size == N64_SZ8)
        {
            if ((int64_t)imm < -128 || (int64_t)imm > 255)
            {
                return false;
            }
            N64EncRex(b, rex, r->NeedsRex);
            N64BufByte(b, (uint8_t)(0xB0U + (r->Index & 7U)));
            N64BufByte(b, (uint8_t)imm);
            return true;
        }

        if ((int64_t)imm < -2147483648LL || (int64_t)imm > 4294967295LL)
        {
            return false;
        }
        N64EncOpSize16(b, size);
        N64EncRex(b, rex, false);
        N64BufByte(b, (uint8_t)(0xB8U + (r->Index & 7U)));
        EmitImm(b, imm, size);
        return true;
    }

    if (dst->Kind == N64_OP_MEM && src->Kind == N64_OP_IMM)
    {
        const N64MemOp *m = &dst->Mem;
        uint64_t imm = src->Imm;
        size = dst->SizeHint != 0U ? dst->SizeHint : N64_SZ32;
        rex = N64EncMemRexBits(m, size == N64_SZ64 ? N64_REX_W : 0U);

        if (size == N64_SZ8)
        {
            if ((int64_t)imm < -128 || (int64_t)imm > 255)
            {
                return false;
            }
            N64EncRex(b, rex, false);
            N64BufByte(b, 0xC6);
            N64EncModRmMem(b, sink, reloc_base, m, 0);
            N64BufByte(b, (uint8_t)imm);
            return true;
        }
        if (size == N64_SZ16)
        {
            N64EncOpSize16(b, size);
            N64EncRex(b, rex, false);
            N64BufByte(b, 0xC7);
            N64EncModRmMem(b, sink, reloc_base, m, 0);
            N64BufU16(b, (uint16_t)imm);
            return true;
        }
        N64EncRex(b, rex, false);
        N64BufByte(b, 0xC7);
        N64EncModRmMem(b, sink, reloc_base, m, 0);
        N64BufU32(b, (uint32_t)imm);
        return true;
    }

    return false;
}

/* movabs r64, imm64 — also used for every external/symbolic reference:
 *   movabs rax, printf     ->  48 B8 <ABS64 reloc>                    */
bool N64EncMovAbs64(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                    const N64Reg *r, uint64_t imm,
                    const char *sym, int64_t sym_addend)
{
    if (r->Size != N64_SZ64)
    {
        return false;
    }
    N64EncRex(b, N64_REX_W | RegRexB(r), false);
    N64BufByte(b, (uint8_t)(0xB8U + (r->Index & 7U)));
    if (sym != NULL && sym[0] != '\0')
    {
        sink->Add(sink->User, reloc_base + b->Size,
                  N64O_R_ABS64, sym, sym_addend);
    }
    N64BufU64(b, imm);
    return true;
}

/* ------------------------------------------------------------------ */
/* TEST                                                                */
/* ------------------------------------------------------------------ */

bool N64EncTest(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                const N64Operand *dst, const N64Operand *src)
{
    unsigned rex;
    unsigned size;
    bool force;

    if (dst->Kind == N64_OP_REG && src->Kind == N64_OP_REG)
    {
        size = dst->Reg.Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(&src->Reg) | RegRexB(&dst->Reg);
        force = size == N64_SZ8 &&
                (dst->Reg.NeedsRex || src->Reg.NeedsRex);
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, 0x85);
        N64EncModRmRegReg(b, (uint8_t)(src->Reg.Index & 7U), &dst->Reg);
        return true;
    }
    if (dst->Kind == N64_OP_REG && src->Kind == N64_OP_MEM)
    {
        size = dst->Reg.Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(&dst->Reg) |
              N64EncMemRexBits(&src->Mem, 0U);
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, size);
        N64BufByte(b, 0x85);
        N64EncModRmMem(b, sink, reloc_base, &src->Mem,
                       (uint8_t)(dst->Reg.Index & 7U));
        return true;
    }
    return false;
}

bool N64EncTestImm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                   const N64Operand *dst, uint64_t imm)
{
    unsigned size;
    unsigned rex;
    bool force;

    (void)sink;
    (void)reloc_base;

    if (dst->Kind != N64_OP_REG)
    {
        return false;
    }
    size = dst->Reg.Size;
    rex = (size == N64_SZ64 ? N64_REX_W : 0U) | RegRexB(&dst->Reg);
    force = size == N64_SZ8 && dst->Reg.NeedsRex;

    if (dst->Reg.Index == 0 && !force)
    {
        if (size == N64_SZ8)
        {
            N64BufByte(b, 0xA8);
            N64BufByte(b, (uint8_t)imm);
            return true;
        }
        N64EncOpSize16(b, size);
        N64EncRex(b, size == N64_SZ64 ? N64_REX_W : 0U, false);
        N64BufByte(b, 0xA9);
        EmitImm(b, imm, size == N64_SZ16 ? N64_SZ16 : N64_SZ32);
        return true;
    }

    N64EncOpSize16(b, size);
    N64EncRex(b, rex, force);
    if (size == N64_SZ8)
    {
        N64BufByte(b, 0xF6);
    }
    else
    {
        N64BufByte(b, 0xF7);
    }
    N64EncModRmRegReg(b, 0, &dst->Reg);
    EmitImm(b, imm, size == N64_SZ16 ? N64_SZ16 :
                        (size == N64_SZ8 ? N64_SZ8 : N64_SZ32));
    return true;
}

/* ------------------------------------------------------------------ */
/* unary group F6/F7                                                   */
/* NOT=/2 NEG=/3 MUL=/4 IMUL=/5 DIV=/6 IDIV=/7                         */
/* ------------------------------------------------------------------ */

bool N64EncUnaryGroup(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                      uint8_t ext_index, const N64Operand *op)
{
    unsigned size;
    unsigned rex;
    bool force;

    if (op->Kind == N64_OP_REG)
    {
        size = op->Reg.Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) | RegRexB(&op->Reg);
        force = size == N64_SZ8 && op->Reg.NeedsRex;
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, size == N64_SZ8 ? 0xF6 : 0xF7);
        N64EncModRmRegReg(b, ext_index, &op->Reg);
        return true;
    }
    if (op->Kind == N64_OP_MEM)
    {
        size = op->SizeHint != 0U ? op->SizeHint : N64_SZ32;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              N64EncMemRexBits(&op->Mem, 0U);
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, size);
        N64BufByte(b, size == N64_SZ8 ? 0xF6 : 0xF7);
        N64EncModRmMem(b, sink, reloc_base, &op->Mem, ext_index);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* INC / DEC (FF /0, FF /1; FE for byte)                               */
/* ------------------------------------------------------------------ */

bool N64EncIncDec(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const N64Operand *op, bool is_inc)
{
    unsigned size;
    unsigned rex;
    bool force;

    if (op->Kind == N64_OP_REG)
    {
        size = op->Reg.Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) | RegRexB(&op->Reg);
        force = size == N64_SZ8 && op->Reg.NeedsRex;
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, size == N64_SZ8 ? 0xFE : 0xFF);
        N64EncModRmRegReg(b, is_inc ? 0 : 1, &op->Reg);
        return true;
    }
    if (op->Kind == N64_OP_MEM)
    {
        size = op->SizeHint != 0U ? op->SizeHint : N64_SZ32;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              N64EncMemRexBits(&op->Mem, 0U);
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, size);
        N64BufByte(b, size == N64_SZ8 ? 0xFE : 0xFF);
        N64EncModRmMem(b, sink, reloc_base, &op->Mem, is_inc ? 0 : 1);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* shifts/rotates: SHL/ROL=/0 SHR=/1 RCL=/2 RCR=/3 ROL=/4 RCR...       */
/* SHL=/4 SHR=/5 SAR=/7 (ROL=0 ROR=1 RCL=2 RCR=3)                      */
/* ------------------------------------------------------------------ */

bool N64EncShiftImm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                    uint8_t ext_index, const N64Operand *op,
                    uint64_t count)
{
    unsigned size;
    unsigned rex;
    bool force;

    if (op->Kind == N64_OP_REG)
    {
        size = op->Reg.Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) | RegRexB(&op->Reg);
        force = size == N64_SZ8 && op->Reg.NeedsRex;

        if (count == 1)
        {
            N64EncRex(b, rex, force);
            N64EncOpSize16(b, size);
            N64BufByte(b, size == N64_SZ8 ? 0xD0 : 0xD1);
            N64EncModRmRegReg(b, ext_index, &op->Reg);
            return true;
        }
        if (count > 63)
        {
            return false;
        }
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, size == N64_SZ8 ? 0xC0 : 0xC1);
        N64EncModRmRegReg(b, ext_index, &op->Reg);
        N64BufByte(b, (uint8_t)count);
        return true;
    }

    if (op->Kind == N64_OP_MEM)
    {
        size = op->SizeHint != 0U ? op->SizeHint : N64_SZ32;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              N64EncMemRexBits(&op->Mem, 0U);
        force = false;
        if (count == 1)
        {
            N64EncRex(b, rex, force);
            N64EncOpSize16(b, size);
            N64BufByte(b, size == N64_SZ8 ? 0xD0 : 0xD1);
            N64EncModRmMem(b, sink, reloc_base, &op->Mem, ext_index);
            return true;
        }
        if (count > 63)
        {
            return false;
        }
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, size == N64_SZ8 ? 0xC0 : 0xC1);
        N64EncModRmMem(b, sink, reloc_base, &op->Mem, ext_index);
        N64BufByte(b, (uint8_t)count);
        return true;
    }

    return false;
}

bool N64EncShiftCl(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                   uint8_t ext_index, const N64Operand *op)
{
    unsigned size;
    unsigned rex;
    bool force;

    if (op->Kind == N64_OP_REG)
    {
        size = op->Reg.Size;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) | RegRexB(&op->Reg);
        force = size == N64_SZ8 && op->Reg.NeedsRex;
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, size == N64_SZ8 ? 0xD2 : 0xD3);
        N64EncModRmRegReg(b, ext_index, &op->Reg);
        return true;
    }
    if (op->Kind == N64_OP_MEM)
    {
        size = op->SizeHint != 0U ? op->SizeHint : N64_SZ32;
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              N64EncMemRexBits(&op->Mem, 0U);
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, size);
        N64BufByte(b, size == N64_SZ8 ? 0xD2 : 0xD3);
        N64EncModRmMem(b, sink, reloc_base, &op->Mem, ext_index);
        return true;
    }
    return false;
}

bool N64EncShift1(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  uint8_t ext_index, const N64Operand *op)
{
    return N64EncShiftImm(b, sink, reloc_base, ext_index, op, 1);
}

/* ------------------------------------------------------------------ */
/* LEA                                                                 */
/* ------------------------------------------------------------------ */

bool N64EncLea(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
               const N64Reg *dst, const N64MemOp *m)
{
    unsigned rex = RegRexR(dst) | N64EncMemRexBits(m, 0U);
    if (dst->Size == N64_SZ64)
    {
        rex |= N64_REX_W;
    }
    else if (dst->Size != N64_SZ32)
    {
        return false;
    }
    N64EncRex(b, rex, false);
    N64BufByte(b, 0x8D);
    N64EncModRmMem(b, sink, reloc_base, m, (uint8_t)(dst->Index & 7U));
    return true;
}

/* ------------------------------------------------------------------ */
/* PUSH / POP                                                          */
/* ------------------------------------------------------------------ */

bool N64EncPushReg(N64Buf *b, const N64Reg *r)
{
    if (r->Size != N64_SZ64)
    {
        return false;
    }
    N64EncRex(b, RegRexB(r), false);
    N64BufByte(b, (uint8_t)(0x50U + (r->Index & 7U)));
    return true;
}

bool N64EncPopReg(N64Buf *b, const N64Reg *r)
{
    if (r->Size != N64_SZ64)
    {
        return false;
    }
    N64EncRex(b, RegRexB(r), false);
    N64BufByte(b, (uint8_t)(0x58U + (r->Index & 7U)));
    return true;
}

bool N64EncPushImm(N64Buf *b, uint64_t imm)
{
    if ((int64_t)imm >= -128 && (int64_t)imm <= 127)
    {
        N64BufByte(b, 0x6A);
        N64BufByte(b, (uint8_t)(int8_t)imm);
        return true;
    }
    if ((int64_t)imm < -2147483648LL || (int64_t)imm > 4294967295LL)
    {
        return false;
    }
    N64BufByte(b, 0x68);
    N64BufU32(b, (uint32_t)imm);
    return true;
}

bool N64EncPopMem(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const N64MemOp *m)
{
    N64EncRex(b, N64_REX_W | N64EncMemRexBits(m, 0U), false);
    N64BufByte(b, 0x8F);
    N64EncModRmMem(b, sink, reloc_base, m, 0);
    return true;
}

bool N64EncPushMem(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                   const N64MemOp *m)
{
    N64EncRex(b, N64_REX_W | N64EncMemRexBits(m, 0U), false);
    N64BufByte(b, 0xFF);
    N64EncModRmMem(b, sink, reloc_base, m, 6); /* FF /6 */
    return true;
}

/* ------------------------------------------------------------------ */
/* XCHG: 86 (8-bit r/m,r) / 87 (16/32/64 r/m,r)                        */
/* ------------------------------------------------------------------ */

bool N64EncXchg(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                const N64Operand *op0, const N64Operand *op1)
{
    const N64MemOp *mem;
    const N64Reg *r;
    bool rev = false;
    unsigned size;
    unsigned rex;
    bool force;

    if (op0->Kind == N64_OP_REG && op1->Kind == N64_OP_REG)
    {
        size = op0->Reg.Size;
        if (op1->Reg.Size != size)
        {
            return false;
        }
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexB(&op0->Reg) | RegRexR(&op1->Reg);
        force = size == N64_SZ8 &&
                (op0->Reg.NeedsRex || op1->Reg.NeedsRex);
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, size == N64_SZ8 ? 0x86 : 0x87);
        N64EncModRmRegReg(b, (uint8_t)(op1->Reg.Index & 7U), &op0->Reg);
        return true;
    }
    if (op0->Kind == N64_OP_REG && op1->Kind == N64_OP_MEM)
    {
        r = &op0->Reg;
        mem = &op1->Mem;
    }
    else if (op0->Kind == N64_OP_MEM && op1->Kind == N64_OP_REG)
    {
        r = &op1->Reg;
        mem = &op0->Mem;
        rev = true;
    }
    else
    {
        return false;
    }
    size = r->Size;
    if (size == N64_SZ64)
    {
        rex = N64_REX_W | RegRexR(r) | N64EncMemRexBits(mem, 0U);
    }
    else
    {
        rex = RegRexR(r) | N64EncMemRexBits(mem, size == N64_SZ8 ? 0U : 0U);
    }
    force = size == N64_SZ8 &&
            (r->NeedsRex || mem->BaseIdx >= 8 || mem->IndexIdx >= 8);
    N64EncRex(b, rex, force);
    N64EncOpSize16(b, size);
    N64BufByte(b, size == N64_SZ8 ? 0x86 : 0x87);
    N64EncModRmMem(b, sink, reloc_base, mem, (uint8_t)(r->Index & 7U));
    (void)rev; /* xchg is symmetric */
    return true;
}

/* ------------------------------------------------------------------ */
/* CMOVcc r, r/m: 0F 40+cc  (16/32/64)                                 */
/* ------------------------------------------------------------------ */

bool N64EncCmovcc(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  uint8_t cc, const N64Operand *dst, const N64Operand *src)
{
    unsigned size;
    unsigned rex;
    bool force;
    const N64Reg *r;

    if (dst->Kind != N64_OP_REG || dst->Reg.Size == N64_SZ8)
    {
        return false;
    }
    r = &dst->Reg;
    size = r->Size;

    if (src->Kind == N64_OP_REG)
    {
        if (src->Reg.Size != size)
        {
            return false;
        }
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexB(r) | RegRexR(&src->Reg);
        force = false;
        N64EncRex(b, rex, force);
        N64EncOpSize16(b, size);
        N64BufByte(b, 0x0F);
        N64BufByte(b, (uint8_t)(0x40U + cc));
        N64EncModRmRegReg(b, (uint8_t)(src->Reg.Index & 7U), r);
        return true;
    }
    if (src->Kind == N64_OP_MEM)
    {
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexB(r) | N64EncMemRexBits(&src->Mem, 0U);
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, size);
        N64BufByte(b, 0x0F);
        N64BufByte(b, (uint8_t)(0x40U + cc));
        N64EncModRmMem(b, sink, reloc_base, &src->Mem,
                       (uint8_t)(r->Index & 7U));
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* BSF r, r/m (0F BC) / BSR r, r/m (0F BD)                             */
/* ------------------------------------------------------------------ */

bool N64EncBsfBsr(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  bool is_bsf, const N64Operand *dst, const N64Operand *src)
{
    unsigned size;
    unsigned rex;
    const N64Reg *r;

    if (dst->Kind != N64_OP_REG || dst->Reg.Size == N64_SZ8)
    {
        return false;
    }
    r = &dst->Reg;
    size = r->Size;

    if (src->Kind == N64_OP_REG)
    {
        if (src->Reg.Size != size)
        {
            return false;
        }
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexB(r) | RegRexR(&src->Reg);
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, size);
        N64BufByte(b, 0x0F);
        N64BufByte(b, is_bsf ? 0xBC : 0xBD);
        N64EncModRmRegReg(b, (uint8_t)(src->Reg.Index & 7U), r);
        return true;
    }
    if (src->Kind == N64_OP_MEM)
    {
        rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexB(r) | N64EncMemRexBits(&src->Mem, 0U);
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, size);
        N64BufByte(b, 0x0F);
        N64BufByte(b, is_bsf ? 0xBC : 0xBD);
        N64EncModRmMem(b, sink, reloc_base, &src->Mem,
                       (uint8_t)(r->Index & 7U));
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* BT family: BT=/0 BTS=/1 BTR=/2 BTC=/3                               */
/*   r/m,r    : 0F A3/AB/B3/BB                                          */
/*   r/m,imm8 : 0F BA /4..7 ib                                         */
/* ------------------------------------------------------------------ */

static const uint8_t N64BtRmOpcode[] = { 0xA3, 0xAB, 0xB3, 0xBB };

bool N64EncBt(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
              uint8_t ext_index, const N64Operand *dst, const N64Operand *src)
{
    unsigned size;
    unsigned rex;
    bool mem_dst;
    const N64MemOp *m;
    const N64Reg *r;

    if (ext_index > 3)
    {
        return false;
    }
    if ((dst->Kind != N64_OP_REG && dst->Kind != N64_OP_MEM) ||
        (dst->Kind == N64_OP_REG && dst->Reg.Size == N64_SZ8))
    {
        return false;
    }
    size = dst->Kind == N64_OP_REG ? dst->Reg.Size :
           (dst->SizeHint != 0U ? dst->SizeHint : N64_SZ32);
    mem_dst = (dst->Kind == N64_OP_MEM);
    m = mem_dst ? &dst->Mem : NULL;
    r = mem_dst ? NULL : &dst->Reg;

    if (src->Kind == N64_OP_REG)
    {
        if (src->Reg.Size != size)
        {
            return false;
        }
        if (mem_dst)
        {
            rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
                  RegRexR(&src->Reg) | N64EncMemRexBits(m, 0U);
            N64EncRex(b, rex, false);
            N64EncOpSize16(b, size);
            N64BufByte(b, 0x0F);
            N64BufByte(b, N64BtRmOpcode[ext_index]);
            N64EncModRmMem(b, sink, reloc_base, m,
                           (uint8_t)(src->Reg.Index & 7U));
        }
        else
        {
            rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
                  RegRexB(r) | RegRexR(&src->Reg);
            N64EncRex(b, rex, false);
            N64EncOpSize16(b, size);
            N64BufByte(b, 0x0F);
            N64BufByte(b, N64BtRmOpcode[ext_index]);
            N64EncModRmRegReg(b, (uint8_t)(src->Reg.Index & 7U), r);
        }
        return true;
    }
    if (src->Kind == N64_OP_IMM && src->Sym[0] == '\0')
    {
        uint64_t imm = src->Imm;
        if (imm > 255)
        {
            return false;
        }
        if (mem_dst)
        {
            rex = (size == N64_SZ64 ? N64_REX_W : 0U) |
                  N64EncMemRexBits(m, 0U);
            N64EncRex(b, rex, false);
            N64EncOpSize16(b, size);
            N64BufByte(b, 0x0F);
            N64BufByte(b, 0xBA);
            N64EncModRmMem(b, sink, reloc_base, m, (uint8_t)(4U + ext_index));
            N64BufByte(b, (uint8_t)imm);
        }
        else
        {
            rex = (size == N64_SZ64 ? N64_REX_W : 0U) | RegRexB(r);
            N64EncRex(b, rex, false);
            N64EncOpSize16(b, size);
            N64BufByte(b, 0x0F);
            N64BufByte(b, 0xBA);
            N64EncModRmRegReg(b, (uint8_t)(4U + ext_index), r);
            N64BufByte(b, (uint8_t)imm);
        }
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* control flow                                                        */
/* ------------------------------------------------------------------ */

static void EmitRel32(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                      const char *sym, int64_t addend, uint64_t abs_target,
                      bool has_abs)
{
    if (!has_abs && sym != NULL && sym[0] != '\0')
    {
        sink->Add(sink->User, reloc_base + b->Size,
                  N64O_R_REL32, sym, addend);
    }
    /* placeholder; linker patches with S - (P + 4) */
    N64BufU32(b, has_abs ? (uint32_t)(int32_t)(abs_target) : 0);
}

bool N64EncCallRel(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                   const char *sym, int64_t addend, uint64_t abs_target,
                   bool has_abs)
{
    N64BufByte(b, 0xE8);
    EmitRel32(b, sink, reloc_base, sym, addend, abs_target, has_abs);
    return true;
}

bool N64EncJmpRel(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const char *sym, int64_t addend, uint64_t abs_target,
                  bool has_abs)
{
    N64BufByte(b, 0xE9);
    EmitRel32(b, sink, reloc_base, sym, addend, abs_target, has_abs);
    return true;
}

bool N64EncCallRm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const N64Operand *op)
{
    if (op->Kind == N64_OP_REG)
    {
        N64EncRex(b, RegRexB(&op->Reg), false);
        N64BufByte(b, 0xFF);
        N64EncModRmRegReg(b, 2, &op->Reg); /* call r/m64 */
        return true;
    }
    if (op->Kind == N64_OP_MEM)
    {
        N64EncRex(b, N64EncMemRexBits(&op->Mem, 0U), false);
        N64BufByte(b, 0xFF);
        N64EncModRmMem(b, sink, reloc_base, &op->Mem, 2);
        return true;
    }
    return false;
}

bool N64EncJmpRm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                 const N64Operand *op)
{
    if (op->Kind == N64_OP_REG)
    {
        N64EncRex(b, RegRexB(&op->Reg), false);
        N64BufByte(b, 0xFF);
        N64EncModRmRegReg(b, 4, &op->Reg); /* jmp r/m64 */
        return true;
    }
    if (op->Kind == N64_OP_MEM)
    {
        N64EncRex(b, N64EncMemRexBits(&op->Mem, 0U), false);
        N64BufByte(b, 0xFF);
        N64EncModRmMem(b, sink, reloc_base, &op->Mem, 4);
        return true;
    }
    return false;
}

bool N64EncJcc(N64Buf *b, N64RelocSink *sink, uint32_t rel_base,
               uint8_t cc, const char *sym, int64_t addend,
               uint64_t abs_target, bool has_abs)
{
    if (!has_abs && sym != NULL && sym[0] != '\0')
    {
        /* Conditional jumps cannot take an indirect target.  Emit the
         * canonical trampoline:
         *     j<cc^1> skip            ; inverted condition, rel32
         *     movabs rax, sym         ; ABS64 relocation
         *     jmp rax
         * skip:                                                            */
        N64BufByte(b, 0x0F);
        N64BufByte(b, (uint8_t)(0x80U + (cc ^ 1U)));
        N64BufU32(b, 12); /* skip the next 10+2 bytes */

        {
            N64Reg tmp;
            tmp.Index = 0;   /* rax scratch */
            tmp.Size = N64_SZ64;
            tmp.NeedsRex = false;
            (void)N64EncMovAbs64(b, sink, rel_base, &tmp, 0, sym, addend);
        }
        N64BufByte(b, 0xFF); /* jmp rax */
        N64BufByte(b, 0xE0);
        return true;
    }

    N64BufByte(b, 0x0F);
    N64BufByte(b, (uint8_t)(0x80U + cc));
    EmitRel32(b, sink, rel_base, NULL, 0, abs_target, has_abs);
    return true;
}

/* ------------------------------------------------------------------ */
/* SETcc                                                               */
/* ------------------------------------------------------------------ */

bool N64EncSetcc(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                 uint8_t cc, const N64Operand *op)
{
    unsigned rex;
    bool force;

    if (op->Kind == N64_OP_REG)
    {
        if (op->Reg.Size != N64_SZ8)
        {
            return false;
        }
        rex = RegRexB(&op->Reg);
        force = op->Reg.NeedsRex;
        N64EncRex(b, rex, force);
        N64BufByte(b, 0x0F);
        N64BufByte(b, (uint8_t)(0x90U + cc));
        N64EncModRmRegReg(b, 0, &op->Reg);
        return true;
    }
    if (op->Kind == N64_OP_MEM)
    {
        rex = N64EncMemRexBits(&op->Mem, 0U);
        N64EncRex(b, rex, false);
        N64BufByte(b, 0x0F);
        N64BufByte(b, (uint8_t)(0x90U + cc));
        N64EncModRmMem(b, sink, reloc_base, &op->Mem, 0);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* MOVZX / MOVSX                                                       */
/* ------------------------------------------------------------------ */

bool N64EncMovx(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                bool is_signed, const N64Reg *dst, const N64Operand *src)
{
    unsigned rex = (dst->Size == N64_SZ64 ? N64_REX_W : 0U) |
                   RegRexR(dst);

    if (src->Kind == N64_OP_REG)
    {
        rex |= RegRexB(&src->Reg);
        N64EncRex(b, rex, src->Reg.NeedsRex || dst->NeedsRex);
        if (is_signed)
        {
            if (src->Reg.Size == N64_SZ8)
            {
                N64BufByte(b, 0x0F); N64BufByte(b, 0xBE);
            }
            else if (src->Reg.Size == N64_SZ16)
            {
                N64BufByte(b, 0x0F); N64BufByte(b, 0xBF);
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (src->Reg.Size == N64_SZ8)
            {
                N64BufByte(b, 0x0F); N64BufByte(b, 0xB6);
            }
            else if (src->Reg.Size == N64_SZ16)
            {
                N64BufByte(b, 0x0F); N64BufByte(b, 0xB7);
            }
            else
            {
                return false;
            }
        }
        N64EncModRmRegReg(b, (uint8_t)(dst->Index & 7U), &src->Reg);
        return true;
    }

    if (src->Kind == N64_OP_MEM)
    {
        unsigned sw;
        bool is16;
        sw = src->SizeHint != 0U ? src->SizeHint : N64_SZ8;
        is16 = (sw == N64_SZ16);
        rex |= N64EncMemRexBits(&src->Mem, 0U);
        N64EncRex(b, rex, dst->NeedsRex);
        if (is_signed)
        {
            N64BufByte(b, 0x0F);
            N64BufByte(b, is16 ? 0xBF : 0xBE);
        }
        else
        {
            N64BufByte(b, 0x0F);
            N64BufByte(b, is16 ? 0xB7 : 0xB6);
        }
        N64EncModRmMem(b, sink, reloc_base, &src->Mem,
                       (uint8_t)(dst->Index & 7U));
        return true;
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* IMUL                                                                */
/* ------------------------------------------------------------------ */

bool N64EncImulRR(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const N64Reg *dst, const N64Operand *src)
{
    unsigned rex;

    if (src->Kind == N64_OP_REG && src->Reg.Size == dst->Size)
    {
        rex = (dst->Size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(dst) | RegRexB(&src->Reg);
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, dst->Size);
        N64BufByte(b, 0x0F);
        N64BufByte(b, 0xAF);
        N64EncModRmRegReg(b, (uint8_t)(dst->Index & 7U), &src->Reg);
        return true;
    }
    if (src->Kind == N64_OP_MEM)
    {
        rex = (dst->Size == N64_SZ64 ? N64_REX_W : 0U) |
              RegRexR(dst) | N64EncMemRexBits(&src->Mem, 0U);
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, dst->Size);
        N64BufByte(b, 0x0F);
        N64BufByte(b, 0xAF);
        N64EncModRmMem(b, sink, reloc_base, &src->Mem,
                       (uint8_t)(dst->Index & 7U));
        return true;
    }
    return false;
}

bool N64EncImulRRI(N64Buf *b, const N64Reg *dst, const N64Reg *src,
                   uint64_t imm)
{
    unsigned rex =
        (dst->Size == N64_SZ64 ? N64_REX_W : 0U) |
        RegRexR(src) | RegRexB(dst);

    if (dst->Size != src->Size)
    {
        return false;
    }
    if ((int64_t)imm >= -128 && (int64_t)imm <= 127)
    {
        N64EncRex(b, rex, false);
        N64EncOpSize16(b, dst->Size);
        N64BufByte(b, 0x6B);
        N64EncModRmRegReg(b, (uint8_t)(src->Index & 7U), dst);
        N64BufByte(b, (uint8_t)(int8_t)imm);
        return true;
    }
    if ((int64_t)imm < -2147483648LL || (int64_t)imm > 4294967295LL)
    {
        return false;
    }
    N64EncRex(b, rex, false);
    N64EncOpSize16(b, dst->Size);
    N64BufByte(b, 0x69);
    N64EncModRmRegReg(b, (uint8_t)(src->Index & 7U), dst);
    N64BufU32(b, (uint32_t)imm);
    return true;
}

/* ------------------------------------------------------------------ */
/* fixed instructions                                                  */
/* ------------------------------------------------------------------ */

void N64EncRet(N64Buf *b, uint64_t imm, bool has_imm)
{
    if (has_imm && imm != 0)
    {
        N64BufByte(b, 0xC2);
        N64BufU16(b, (uint16_t)imm);
        return;
    }
    N64BufByte(b, 0xC3);
}

void N64EncLeave(N64Buf *b)
{
    N64BufByte(b, 0xC9);
}

void N64EncNop(N64Buf *b)
{
    N64BufByte(b, 0x90);
}

void N64EncCdqCqo(N64Buf *b, bool w64)
{
    /* CDQ sign-extends EAX into EDX:EAX (99 without REX.W).
     * CQO sign-extends RAX into RDX:RAX (99 with REX.W).             */
    N64EncRex(b, w64 ? N64_REX_W : 0U, false);
    N64BufByte(b, 0x99);
}

void N64EncCwdeCdqe(N64Buf *b, bool w64)
{
    /* CWDE: AX -> EAX (98 without REX.W).  With REX.W: CDQE EAX -> RAX. */
    N64EncRex(b, w64 ? N64_REX_W : 0U, false);
    N64BufByte(b, 0x98);
}

void N64EncByteInst(N64Buf *b, uint8_t op)
{
    N64BufByte(b, op);
}
