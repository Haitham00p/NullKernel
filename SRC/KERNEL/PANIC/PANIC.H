#ifndef NULLOS_PANIC_H
#define NULLOS_PANIC_H

#include <stdint.h>

void PanicKernel(const char *Reason, uintptr_t FaultAddress)
    __attribute__((noreturn));

#endif
