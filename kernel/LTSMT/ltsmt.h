#pragma once

#include <stdint.h>

#define LTSM_MAX_STATES 64U

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

/*
 * Snapshot of an exception/interrupt return frame.
 * This is the reliable source for the faulting RIP/RSP/RFLAGS.
 */
typedef struct
{
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} LTSM_TRAPSTATE;

typedef struct
{
    const char *name;
    LevelStates level;
    uint64_t value;
    uint64_t warn_at;
    uint64_t failure_at;
    uint64_t critical_at;
    uint64_t corrupt_at;
    uint8_t active;
    uint64_t changes;
} LTSM_STATE;

typedef void (*LTSM_TRIGGER)(const LTSM_STATE *state);

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
void LTSM_ScanTrapState(const void *frame, LTSM_TRAPSTATE *state);

void LTSM_Initialize(void);
int LTSM_RegisterState(LTSM_STATE *state);
int LTSM_SetState(LTSM_STATE *state, LevelStates level, uint64_t value);
int LTSM_UpdateState(LTSM_STATE *state, uint64_t value);
void LTSM_Monitor(void);
void LTSM_MonitorState(LTSM_STATE *state);

LevelStates LTSM_LevelFromValue(const LTSM_STATE *state, uint64_t value);
const char *LTSM_LevelName(LevelStates level);
uint32_t LTSM_StateCount(void);
const LTSM_STATE *LTSM_GetState(uint32_t index);

void LTSM_SetTrigger(LTSM_TRIGGER trigger);
void LTSM_DefaultTrigger(const LTSM_STATE *state);
