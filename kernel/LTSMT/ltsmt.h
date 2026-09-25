#pragma once

#include <stdint.h>

typedef enum
{
    LT_OK = 0,
    LT_WARN,
    LT_FAILURE,
    LT_CRITICAL,
    LT_CORRUPT
} LevelStates;

typedef struct
{
    uint8_t CF;
    uint8_t PF;
    uint8_t AF;
    uint8_t ZF;
    uint8_t SF;
    uint8_t TF;
    uint8_t IF;
    uint8_t DF;
    uint8_t OF;

    uint8_t IOPL;
    uint8_t NT;
    uint8_t RF;
    uint8_t VM;
    uint8_t AC;
    uint8_t VIF;
    uint8_t VIP;
    uint8_t ID;
} LTSM_RFLAGS;

typedef struct
{
    uint64_t rflags;
    uint64_t cr0;
    uint64_t cr2;
    uint64_t cr3;
    uint64_t cr4;

    uint64_t rsp;
    uint64_t rbp;

    uint16_t cs;
    uint16_t ss;

    LTSM_RFLAGS flags;
} LTSM_CPUSTATE;

uint64_t LTSM_ReadRFLAGS(void);
uint64_t LTSM_ReadCR0(void);
uint64_t LTSM_ReadCR2(void);
uint64_t LTSM_ReadCR3(void);
uint64_t LTSM_ReadCR4(void);
uint64_t LTSM_ReadRSP(void);
uint64_t LTSM_ReadRBP(void);
uint16_t LTSM_ReadCS(void);
uint16_t LTSM_ReadSS(void);

void LTSM_ScanRFLAGS(uint64_t rflags, LTSM_RFLAGS *flags);
void LTSM_ScanCPUST(LTSM_CPUSTATE *cpust);
