#ifndef POWER_H
#define POWER_H

#define REBOOT_COMMAND_VALUE 0xFE
#define REBOOT_PORT 0x64
#define SHUTDOWN_COMMAND_VALUE 0x2000
#define SHUTDOWN_PORT 0x604
#define SecSHUTDOWN_PORT 0xB004

void Reboot(void);
void Shutdown(void);

#endif