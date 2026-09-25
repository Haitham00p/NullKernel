#include "ltsmt.h"
#include "kernel/debug/debug.h"

static LTSM_STATE *States[LTSM_MAX_STATES];
static uint32_t StateCount;
static LTSM_TRIGGER Trigger = LTSM_DefaultTrigger;
static uint8_t Initialized;

static int LTSM_IsHigher(LevelStates a, LevelStates b)
{
    return (int)a > (int)b;
}

const char *LTSM_LevelName(LevelStates level)
{
    switch (level)
    {
        case LT_OK:       return "OK";
        case LT_WARN:     return "WARNING";
        case LT_FAILURE:  return "FAILURE";
        case LT_CRITICAL: return "CRITICAL";
        case LT_CORRUPT:  return "CORRUPT";
        default:          return "UNKNOWN";
    }
}

void LTSM_ScanRFLAGS(uint64_t rflags, LTSM_RFLAGS *flags)
{
    if (flags == 0)
        return;

    flags->CF   = (uint8_t)((rflags >> 0)  & 1U);
    flags->PF   = (uint8_t)((rflags >> 2)  & 1U);
    flags->AF   = (uint8_t)((rflags >> 4)  & 1U);
    flags->ZF   = (uint8_t)((rflags >> 6)  & 1U);
    flags->SF   = (uint8_t)((rflags >> 7)  & 1U);
    flags->TF   = (uint8_t)((rflags >> 8)  & 1U);
    flags->IF   = (uint8_t)((rflags >> 9)  & 1U);
    flags->DF   = (uint8_t)((rflags >> 10) & 1U);
    flags->OF   = (uint8_t)((rflags >> 11) & 1U);
    flags->IOPL = (uint8_t)((rflags >> 12) & 3U);
    flags->NT   = (uint8_t)((rflags >> 14) & 1U);
    flags->RF   = (uint8_t)((rflags >> 16) & 1U);
    flags->VM   = (uint8_t)((rflags >> 17) & 1U);
    flags->AC   = (uint8_t)((rflags >> 18) & 1U);
    flags->VIF  = (uint8_t)((rflags >> 19) & 1U);
    flags->VIP  = (uint8_t)((rflags >> 20) & 1U);
    flags->ID   = (uint8_t)((rflags >> 21) & 1U);
}

void LTSM_ScanCPUST(LTSM_CPUSTATE *cpust)
{
    if (cpust == 0)
        return;

    cpust->rflags = LTSM_ReadRFLAGS();
    cpust->cr0    = LTSM_ReadCR0();
    cpust->cr2    = LTSM_ReadCR2();
    cpust->cr3    = LTSM_ReadCR3();
    cpust->cr4    = LTSM_ReadCR4();
    cpust->rsp    = LTSM_ReadRSP();
    cpust->rbp    = LTSM_ReadRBP();
    cpust->cs     = LTSM_ReadCS();
    cpust->ss     = LTSM_ReadSS();

    LTSM_ScanRFLAGS(cpust->rflags, &cpust->flags);
}

/*
 * IRQContext is laid out as:
 *   15 GPR qwords, then RIP, CS, RFLAGS, RSP, SS.
 * Keep this function independent of irq.h so LTSMT does not create
 * an interrupt-layer dependency.
 */
void LTSM_ScanTrapState(const void *frame, LTSM_TRAPSTATE *state)
{
    const uint64_t *raw;

    if (frame == 0 || state == 0)
        return;

    raw = (const uint64_t *)frame;
    state->rip    = raw[15];
    state->cs     = raw[16];
    state->rflags = raw[17];
    state->rsp    = raw[18];
    state->ss     = raw[19];
}

void LTSM_Initialize(void)
{
    uint32_t i;

    for (i = 0; i < LTSM_MAX_STATES; i++)
        States[i] = 0;

    StateCount = 0;
    Trigger = LTSM_DefaultTrigger;
    Initialized = 1;
}

int LTSM_RegisterState(LTSM_STATE *state)
{
    if (state == 0)
        return -1;

    if (!Initialized)
        LTSM_Initialize();

    if (StateCount >= LTSM_MAX_STATES)
        return -2;

    if (state->name == 0)
        return -3;

    state->active = 1;
    state->level = LT_OK;
    state->changes = 0;
    States[StateCount++] = state;

    return 0;
}

LevelStates LTSM_LevelFromValue(const LTSM_STATE *state, uint64_t value)
{
    if (state == 0)
        return LT_OK;

    if (value >= state->corrupt_at)
        return LT_CORRUPT;
    if (value >= state->critical_at)
        return LT_CRITICAL;
    if (value >= state->failure_at)
        return LT_FAILURE;
    if (value >= state->warn_at)
        return LT_WARN;

    return LT_OK;
}

void LTSM_SetTrigger(LTSM_TRIGGER trigger)
{
    Trigger = trigger ? trigger : LTSM_DefaultTrigger;
}

void LTSM_DefaultTrigger(const LTSM_STATE *state)
{
    if (state == 0)
        return;

    switch (state->level)
    {
        case LT_WARN:
            DbgWarn(state->name, (uintptr_t)state->value);
            break;
        case LT_FAILURE:
            DbgErr(state->name, (uintptr_t)state->value);
            break;
        case LT_CRITICAL:
            DbgErr(state->name, (uintptr_t)state->value);
            break;
        case LT_CORRUPT:
            DbgErr(state->name, (uintptr_t)state->value);
            break;
        default:
            break;
    }
}

int LTSM_SetState(LTSM_STATE *state, LevelStates level, uint64_t value)
{
    if (state == 0)
        return -1;

    if (!Initialized)
        LTSM_Initialize();

    if (!state->active)
    {
        int rc = LTSM_RegisterState(state);
        if (rc != 0)
            return rc;
    }

    if (state->level != level || state->value != value)
        state->changes++;

    state->level = level;
    state->value = value;

    if (level != LT_OK && Trigger != 0)
        Trigger(state);

    return 0;
}

void LTSM_MonitorState(LTSM_STATE *state)
{
    LevelStates next;

    if (state == 0 || !state->active)
        return;

    next = LTSM_LevelFromValue(state, state->value);

    if (next != state->level)
    {
        state->level = next;
        state->changes++;

        if (next != LT_OK && Trigger != 0)
            Trigger(state);
    }
}

void LTSM_Monitor(void)
{
    uint32_t i;

    if (!Initialized)
        LTSM_Initialize();

    for (i = 0; i < StateCount; i++)
        LTSM_MonitorState(States[i]);
}

int LTSM_UpdateState(LTSM_STATE *state, uint64_t value)
{
    LevelStates next;

    if (state == 0)
        return -1;

    if (!state->active)
    {
        int rc = LTSM_RegisterState(state);
        if (rc != 0)
            return rc;
    }

    state->value = value;
    next = LTSM_LevelFromValue(state, value);

    return LTSM_SetState(state, next, value);
}

uint32_t LTSM_StateCount(void)
{
    return StateCount;
}

const LTSM_STATE *LTSM_GetState(uint32_t index)
{
    if (index >= StateCount)
        return 0;

    return States[index];
}
