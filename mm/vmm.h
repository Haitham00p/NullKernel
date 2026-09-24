#ifndef NULLOS_VMM_H
#define NULLOS_VMM_H
#include <stdint.h>
#include <stdbool.h>
#define VMM_P 0x001ULL
#define VMM_W 0x002ULL
#define VMM_U 0x004ULL
#define VMM_X 0x000ULL
#define VMM_NX (1ULL<<63)
#define VMM_GLOBAL (1ULL<<8)
void VmmInitialize(void);
uint64_t VmmCurrentRoot(void);
uint64_t VmmKernelRoot(void);
bool VmmMapPage(uint64_t Virtual,uint64_t Physical,uint64_t Flags);
bool VmmUnmapPage(uint64_t Virtual,bool FreePhysical);
uint64_t VmmGetPhysical(uint64_t Virtual);
bool VmmMapUserPage(uint64_t Virtual,uint64_t Flags);
void VmmLoadRoot(uint64_t PhysicalRoot);
#endif
