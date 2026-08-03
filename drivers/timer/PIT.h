#ifndef PIT_H
#define PIT_H
#define Freq 100
#include <stdint.h>

uint64_t PITGetTicks(void);
extern uint32_t PromColor;
extern uint32_t BackspaceColor;
uint64_t PITGetMilliseconds(void);

void PITInitialize(uint32_t Frequency);
void PITTick(void);
void PITTickReset(void);
void PITTest(void);
void PITSleepTicks(uint64_t Ticks);
void PITSleepMS(uint64_t Milliseconds);

#endif