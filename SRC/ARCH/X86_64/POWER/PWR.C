#include "pwr.h"
#include "../cpu/io.h"

void Reboot(){
    Outb(REBOOT_PORT, REBOOT_COMMAND_VALUE);
}

void Shutdown(){
    Outw(SHUTDOWN_PORT, SHUTDOWN_COMMAND_VALUE);
}