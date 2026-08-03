#ifndef ISR_H
#define ISR_H

#include <stdint.h>

void ISRInitialize(void);

void ISRHandler(uint32_t InterruptNumber);


extern void ISR0(void);
extern void ISR14(void);

#endif
