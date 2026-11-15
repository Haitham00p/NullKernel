#ifndef NULLOS_BOOTMONITOR_H
#define NULLOS_BOOTMONITOR_H

#include <stdbool.h>
#include <stdint.h>

#define BOOTMONITOR_MODE_ASM      2U
#define BOOTMONITOR_MODE_NPB      3U

bool BootMonitorShouldEnter(void);
void BootMonitorRequest(void);
void BootMonitorClearRequest(void);
void BootMonitorRun(void);
void BootMonitorRunMode(uint8_t Mode);

#endif
