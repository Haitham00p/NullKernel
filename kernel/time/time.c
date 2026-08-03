#include "time.h"
#include "../../drivers/timer/PIT.h"

void KernelSleep(uint64_t Milliseconds){
    PITSleepMS(Milliseconds);
}