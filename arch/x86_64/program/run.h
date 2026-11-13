#ifndef NULLOS_RUN_H
#define NULLOS_RUN_H

#include <stdint.h>

/*
 * Result value used when the user aborts a running program with Ctrl+C.
 * The PIT IRQ splices the interrupted guest frame to RunAbortResume and
 * RunProgramOnStack returns this value to the shell.
 */
#define NULLOS_GUEST_ABORTED 0x2BAD0B5EULL

/*
 * Runs a trusted raw program entry point on a dedicated stack.
 * The current runner executes in ring 0 with the kernel page tables; this is
 * not process isolation and must only run trusted binaries. The caller's
 * stack is restored on return.
 * Interrupts remain enabled while the guest runs.
 * Returns the guest's final RAX (its return value), or
 * NULLOS_GUEST_ABORTED when the user pressed Ctrl+C mid-run.
 */
uint64_t RunProgramOnStack(uint64_t Entry);

/*
 * Set to 1 while the guest program is executing (managed by run.asm),
 * and armed by the timer IRQ when Ctrl+C is requested during a guest run.
 */
extern volatile uint64_t RunGuestActive;
extern volatile uint64_t RunAbortArmed;

/* Stack pointer saved by RunProgramOnStack before the guest runs. */
extern volatile uint64_t RunSavedRsp;

/* Resume point the timer IRQ splices into the guest's iret frame on
   Ctrl+C; returns NULLOS_GUEST_ABORTED to the shell. */
extern void RunAbortResume(void);

#endif /* NULLOS_RUN_H */