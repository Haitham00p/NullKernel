#ifndef NULLOS_PROCESS_H
#define NULLOS_PROCESS_H
#include <stdint.h>
#include <stdbool.h>
typedef struct NullProcess NullProcess;
void ProcessSystemInitialize(void);
NullProcess *ProcessCreate(void);
void ProcessDestroy(NullProcess *p);
NullProcess *ProcessCurrent(void);
uint64_t ProcessCount(void);
uint64_t ProcessCurrentId(void);
uint64_t ProcessCreateAddressSpace(void);
bool ProcessSwitchAddressSpace(NullProcess *p);
uint64_t ProcessCr3(const NullProcess *p);
#endif
