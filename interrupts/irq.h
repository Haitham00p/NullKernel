#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

/*
    Hardware IRQ Numbers
*/

#define IRQ_TIMER 0
#define IRQ_KEYBOARD 1
#define IRQ_CASCADE 2
#define IRQ_COM2 3
#define IRQ_COM1 4
#define IRQ_LPT2 5
#define IRQ_FLOPPY 6
#define IRQ_LPT1 7

#define IRQ_CMOS 8
#define IRQ_FREE1 9
#define IRQ_FREE2 10
#define IRQ_FREE3 11
#define IRQ_MOUSE 12
#define IRQ_FPU 13
#define IRQ_ATA1 14
#define IRQ_ATA2 15

/*
    Initialize Hardware IRQs
*/
void IRQInitialize(void);

/*
    Called from Assembly IRQ Stubs
*/
void IRQHandler(uint32_t IRQNumber);

/*
    Assembly IRQ Entry Points
*/

extern void IRQ0(void);
extern void IRQ1(void);
extern void IRQ2(void);
extern void IRQ3(void);
extern void IRQ4(void);
extern void IRQ5(void);
extern void IRQ6(void);
extern void IRQ7(void);

extern void IRQ8(void);
extern void IRQ9(void);
extern void IRQ10(void);
extern void IRQ11(void);
extern void IRQ12(void);
extern void IRQ13(void);
extern void IRQ14(void);
extern void IRQ15(void);

#endif