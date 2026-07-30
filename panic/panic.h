#ifndef PANIC_H
#define PANIC_H

#include "terminal.h"
#include <stdint.h>

void PanicKernel(char *str, uintptr_t API);

#endif