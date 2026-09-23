#ifndef SNDB_H
#define SNDB_H

#include <stdint.h>

#define PIT_CHANNEL2 0x42
#define PIT_COMMAND  0x43
#define SPEAKER_PORT 0x61

extern uint32_t PromColor;
extern uint32_t BackSpaceColor;

void StartBeep(uint64_t Frequency, uint32_t Last);
void EndBeepEmrg(void);

#endif