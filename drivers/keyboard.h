#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define KB_BUFFER_SIZE 128


typedef struct
{
    bool Shift;
    bool Ctrl;
    bool Alt;

    bool CapsLock;
    bool NumLock;
    bool ScrollLock;

} KB_STATE;


void KbdInitialize(void);

bool KbdAvailable(void);

char KbdReadChar(void);

void KbdFlushBuffer(void);

KB_STATE KbdGetState(void);

void KeyboardInterrupt(void);


#endif
