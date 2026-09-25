#include "ltsmt.h"

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

    /* IOPL occupies two bits: 12 and 13. */
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
