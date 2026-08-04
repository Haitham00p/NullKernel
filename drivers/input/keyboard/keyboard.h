#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define KB_BUFFER_SIZE  128

#define KBD_KEY_LEFT     0x80U
#define KBD_KEY_RIGHT    0x81U
#define KBD_KEY_UP       0x82U
#define KBD_KEY_DOWN     0x83U
#define KBD_KEY_HOME     0x84U
#define KBD_KEY_END      0x85U
#define KBD_KEY_DELETE   0x86U
#define KBD_KEY_PAGE_UP  0x87U
#define KBD_KEY_PAGE_DN  0x88U
#define KBD_KEY_INSERT   0x89U

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
uint8_t KbdReadKey(void);
void KbdFlushBuffer(void);
KB_STATE KbdGetState(void);
void KeyboardInterrupt(void);

#endif /* KEYBOARD_H */
