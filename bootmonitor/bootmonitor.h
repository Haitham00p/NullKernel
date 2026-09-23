#ifndef NULLOS_BOOTMONITOR_H
#define NULLOS_BOOTMONITOR_H

#include <stdbool.h>

bool BootMonitorShouldEnter(void);
void BootMonitorRequest(void);
void BootMonitorClearRequest(void);
void BootMonitorRun(void);

#endif
