#ifndef NULLOS_E1000_H
#define NULLOS_E1000_H

#include <stdbool.h>
#include <stdint.h>

bool E1000Initialize(void);
bool E1000IsReady(void);
bool E1000GetMac(uint8_t Mac[6]);

/* Send/receive one raw Ethernet frame. Polling is intentional for now;
 * interrupt delivery is a later layer. */
bool E1000SendFrame(const void *Data, uint16_t Length);
bool E1000ReceiveFrame(void *Buffer, uint16_t BufferSize, uint16_t *Length);

#endif
