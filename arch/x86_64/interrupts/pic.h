#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* PIC I/O Ports */
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21

#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

/* End Of Interrupt */
#define PIC_EOI 0x20

/* Initialization Control Words */
#define ICW1_ICW4 0x01
#define ICW1_SINGLE 0x02
#define ICW1_INTERVAL4 0x04
#define ICW1_LEVEL 0x08
#define ICW1_INIT 0x10

#define ICW4_8086 0x01
#define ICW4_AUTO 0x02
#define ICW4_BUF_SLAVE 0x08
#define ICW4_BUF_MASTER 0x0C
#define ICW4_SFNM 0x10

extern uint8_t m1;
extern uint8_t m2;

void PICRemap(uint8_t Offset1, uint8_t Offset2);

void PICSendEOI(uint8_t IRQ);

void PICMaskIRQ(uint8_t IRQ);

void PICUnmaskIRQ(uint8_t IRQ);

#endif