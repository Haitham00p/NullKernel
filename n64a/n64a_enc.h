#ifndef N64A_ENC_H
#define N64A_ENC_H

#include "n64a.h"
#include "n64a_reg.h"

/* ------------------------------------------------------------------ */
/* growable byte buffer                                                */
/* ------------------------------------------------------------------ */

typedef struct
{
    uint8_t *Data;
    uint32_t Size;
    uint32_t Cap;
} N64Buf;

void N64BufInit(N64Buf *b);
bool N64BufReserve(N64Buf *b, uint32_t extra);
void N64BufByte(N64Buf *b, uint8_t v);
void N64BufBytes(N64Buf *b, const void *src, uint32_t n);
void N64BufFree(N64Buf *b);

/* little-endian stores */
void N64BufU16(N64Buf *b, uint16_t v);
void N64BufU32(N64Buf *b, uint32_t v);
void N64BufU64(N64Buf *b, uint64_t v);

/* ------------------------------------------------------------------ */
/* operands                                                            */
/* ------------------------------------------------------------------ */

typedef enum
{
    N64_OP_NONE = 0,
    N64_OP_REG,        /* Reg valid                                     */
    N64_OP_IMM,        /* Imm valid (pure numeric)                      */
    N64_OP_MEM         /* Mem valid                                     */
} N64OpKind;

typedef struct
{
    bool     HasBase;
    bool     HasIndex;
    uint8_t  BaseIdx;      /* GPR index 0..15                         */
    uint8_t  IndexIdx;     /* GPR index 0..15                         */
    uint8_t  Scale;        /* 1,2,4,8                                 */
    int64_t  Disp;         /* constant displacement                   */
    char     Sym[N64O_NAME_MAX]; /* symbolic displacement ('' if none) */
    int64_t  SymAddend;
} N64MemOp;

typedef struct
{
    N64OpKind Kind;
    N64Reg    Reg;
    uint64_t  Imm;
    char      Sym[N64O_NAME_MAX]; /* symbolic immediate / jump target   */
    int64_t   SymAddend;
    N64MemOp  Mem;
    unsigned  SizeHint;      /* explicit byte/word/dword/qword width
                                for memory operands (0 = none)          */
} N64Operand;

/* ------------------------------------------------------------------ */
/* relocation sink                                                     */
/* ------------------------------------------------------------------ */

/* The encoder emits relocations for symbolic references through this
 * interface so that encoding stays independent of symbol-table storage.
 * Offsets are relative to the START OF THE TEXT BUFFER being emitted
 * (callers pass their own base offset for data sections). */
typedef struct
{
    void *User;   /* implementation defined (N64aAsm*)                  */
    void (*Add)(void *user, uint32_t offset, uint32_t type,
                const char *name, int64_t addend);
} N64RelocSink;

/* ------------------------------------------------------------------ */
/* instruction encoding forms                                          */
/* ------------------------------------------------------------------ */

#define N64_REX_W 0x08U
#define N64_REX_R 0x04U
#define N64_REX_X 0x02U
#define N64_REX_B 0x01U

/* condition codes for jcc/setcc */
enum
{
    N64_CC_O = 0, N64_CC_NO, N64_CC_B, N64_CC_AE,
    N64_CC_E, N64_CC_NE, N64_CC_BE, N64_CC_A,
    N64_CC_S, N64_CC_NS, N64_CC_P, N64_CC_NP,
    N64_CC_L, N64_CC_GE, N64_CC_LE, N64_CC_G
};

/* Maps a condition mnemonic suffix ("e","ne","g",...) to N64_CC_*.
 * Returns false when CcName is unknown. */
bool N64CondFind(const char *CcName, uint8_t *Out);

/* Emits REX prefix when any bit set or forced (for spl/bpl/sil/dil). */
void N64EncRex(N64Buf *b, unsigned flags, bool force);

/* ModR/M + SIB + displacement for a memory operand.
 * RegField: the /digit or register field (3 bits).                */
void N64EncModRmMem(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                    const N64MemOp *m, uint8_t reg_field);

/* ModR/M for register-register (mod=11). */
void N64EncModRmRegReg(N64Buf *b, uint8_t reg_field, const N64Reg *rm);

/* Full memory-operand REX computation helper (W flag decided by caller). */
unsigned N64EncMemRexBits(const N64MemOp *m, unsigned extra);

/* --- instruction emitters (operands pre-validated by the parser) --- */

/* ADD/OR/ADC/SBB/AND/SUB/XOR/CMP family. OpBase: 0x00 for the 8-bit
 * r/m,r opcode; the family follows the x86 opcode pattern.            */
bool N64EncArithRegMem(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                       uint8_t op_base, const N64Operand *dst,
                       const N64Operand *src);

/* r/m, immediate (uses /digit Ext for the 80/81/83 groups).           */
bool N64EncArithImm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                    uint8_t ext_index, const N64Operand *dst,
                    uint64_t imm, unsigned size, bool dst_is_acc);

/* MOV variants.                                                       */
bool N64EncMov(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
               const N64Operand *dst, const N64Operand *src);
/* MOV r64, imm64 (B8+r + optional ABS64 reloc for symbolic imm).      */
bool N64EncMovAbs64(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                    const N64Reg *r, uint64_t imm,
                    const char *sym, int64_t sym_addend);

/* TEST r/m,r                                                          */
bool N64EncTest(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                const N64Operand *dst, const N64Operand *src);
/* TEST r/m, imm                                                       */
bool N64EncTestImm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                   const N64Operand *dst, uint64_t imm);

/* unary group F6/F7: NOT(/2) NEG(/3) MUL(/4) IMUL(/5) DIV(/6) IDIV(/7) */
bool N64EncUnaryGroup(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                      uint8_t ext_index, const N64Operand *op);

/* INC/DEC (FF /0, FF /1; FE for 8-bit)                                */
bool N64EncIncDec(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const N64Operand *op, bool is_inc);

/* shifts/rotates: SHL=/4 SHR=/5 SAR=/7 (also ROL=0 ROR=1 RCL=2 RCR=3) */
bool N64EncShiftImm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                    uint8_t ext_index, const N64Operand *op,
                    uint64_t count);
bool N64EncShiftCl(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                   uint8_t ext_index, const N64Operand *op);
bool N64EncShift1(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  uint8_t ext_index, const N64Operand *op);

/* XCHG r/m, r (86/87)                                                 */
bool N64EncXchg(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                const N64Operand *op0, const N64Operand *op1);

/* CMOVcc r, r/m (0F 40+cc)                                            */
bool N64EncCmovcc(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  uint8_t cc, const N64Operand *dst,
                  const N64Operand *src);

/* BSF/BSR r, r/m (0F BC / 0F BD)                                      */
bool N64EncBsfBsr(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  bool is_bsf, const N64Operand *dst,
                  const N64Operand *src);

/* BT family: BT=/0 BTS=/1 BTR=/2 BTC=/3  (r/m,r and r/m,imm8)         */
bool N64EncBt(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
              uint8_t ext_index, const N64Operand *dst,
              const N64Operand *src);

/* LEA r64, m                                                          */
bool N64EncLea(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
               const N64Reg *dst, const N64MemOp *m);

/* PUSH/POP                                                            */
bool N64EncPushReg(N64Buf *b, const N64Reg *r);
bool N64EncPopReg(N64Buf *b, const N64Reg *r);
bool N64EncPushImm(N64Buf *b, uint64_t imm);
bool N64EncPushMem(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                   const N64MemOp *m);
bool N64EncPopMem(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const N64MemOp *m);

/* control flow. Symbolic targets produce REL32 relocations through the
 * sink; numeric targets are encoded as rel32 directly.                */
bool N64EncCallRel(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                   const char *sym, int64_t addend, uint64_t abs_target,
                   bool has_abs);
bool N64EncJmpRel(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const char *sym, int64_t addend, uint64_t abs_target,
                  bool has_abs);
bool N64EncCallRm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const N64Operand *op);
bool N64EncJmpRm(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                 const N64Operand *op);

/* conditional jumps. Local targets: rel32/rel8. External (symbolic)
 * targets: inverted-jump trampoline around movabs+jmp rax.            */
bool N64EncJcc(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
               uint8_t cc, const char *sym, int64_t addend,
               uint64_t abs_target, bool has_abs);

/* SETcc r/m8                                                          */
bool N64EncSetcc(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                 uint8_t cc, const N64Operand *op);

/* MOVZX / MOVSX (dest register, src reg-or-mem)                        */
bool N64EncMovx(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                bool is_signed, const N64Reg *dst, const N64Operand *src);

/* IMUL two/three operand forms                                        */
bool N64EncImulRR(N64Buf *b, N64RelocSink *sink, uint32_t reloc_base,
                  const N64Reg *dst, const N64Operand *src);
bool N64EncImulRRI(N64Buf *b, const N64Reg *dst, const N64Reg *src,
                   uint64_t imm);

/* simple single-byte / fixed instructions                             */
void N64EncRet(N64Buf *b, uint64_t imm, bool has_imm);
void N64EncLeave(N64Buf *b);
void N64EncNop(N64Buf *b);
void N64EncCdqCqo(N64Buf *b, bool w64);
void N64EncCwdeCdqe(N64Buf *b, bool w64);
void N64EncByteInst(N64Buf *b, uint8_t op); /* CLI/STI/HLT ...        */

#endif
