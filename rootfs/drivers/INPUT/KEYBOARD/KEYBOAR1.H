#ifndef NULLOS_DRIVERS_KEYBOARD_LAYOUT_H
#define NULLOS_DRIVERS_KEYBOARD_LAYOUT_H

#include <stdint.h>

/*
 * Number of supported Scan Codes.
 * We only use the first Scan Code Set (PS/2 Set 1).
 */
#define KB_LAYOUT_SIZE 128

/*
 * Default keyboard layout.
 *
 * Used when:
 *  - Shift = OFF
 *  - Caps Lock = OFF
 */
extern const char KeyboardLayoutNormal[KB_LAYOUT_SIZE];

/*
 * Shift keyboard layout.
 *
 * Used when:
 *  - Shift = ON
 */
extern const char KeyboardLayoutShift[KB_LAYOUT_SIZE];

#endif /* NULLOS_DRIVERS_KEYBOARD_LAYOUT_H */