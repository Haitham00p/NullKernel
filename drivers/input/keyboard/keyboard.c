#include "keyboard.h"
#include "keyboard_layout.h"
#include "arch/x86_64/cpu/io.h"
#include "kernel/terminal/terminal.h"
#include "drivers/timer/PIT.h"

static uint32_t KbdHead = 0;
static uint32_t KbdTail = 0;

static bool KeyPressed[128] = {0};

static uint8_t KbdBuffer[KB_BUFFER_SIZE];
static bool ExtendedKey;

static KB_STATE KbdState;

static uint8_t  LastScanCode = 0;
static uint64_t KeyHoldStartTime = 0;
static uint64_t LastRepeatTime = 0;

void KbdInitialize(void)
{
    KbdHead = 0;
    KbdTail = 0;

    KbdState.Shift = false;
    KbdState.Ctrl = false;
    KbdState.Alt = false;

    KbdState.CapsLock = false;
    KbdState.NumLock = false;
    KbdState.ScrollLock = false;
    ExtendedKey = false;

    LastScanCode = 0;
    KeyHoldStartTime = 0;
    LastRepeatTime = 0;

    for (uint32_t i = 0; i < KB_BUFFER_SIZE; i++)
    {
        KbdBuffer[i] = 0;
    }
    for (uint32_t i = 0; i < 128; i++)
    {
        KeyPressed[i] = false;
    }   

    while (Inb(0x64) & 0x01)
    {
        Inb(0x60);
    }

    Outb(0x64, 0xAE);
}

static void KbdBufferPush(uint8_t Character)
{
    uint32_t NextHead = (KbdHead + 1) % KB_BUFFER_SIZE;

    if (NextHead == KbdTail)
    {
        return;
    }

    KbdBuffer[KbdHead] = Character;
    KbdHead = NextHead;
}

static uint8_t KbdBufferPop(void)
{
    if (KbdHead == KbdTail)
    {
        return 0;
    }

    char Character = KbdBuffer[KbdTail];

    KbdTail = (KbdTail + 1) % KB_BUFFER_SIZE;

    return Character;
}

bool KbdAvailable(void)
{
    return (KbdHead != KbdTail);
}

char KbdReadChar(void)
{
    return (char)KbdBufferPop();
}

uint8_t KbdReadKey(void)
{
    return KbdBufferPop();
}

void KbdFlushBuffer(void)
{
    KbdHead = 0;
    KbdTail = 0;
}

KB_STATE KbdGetState(void)
{
    return KbdState;
}

void KeyboardInterrupt(void)
{
    uint8_t ScanCode = Inb(0x60);

    if (ScanCode == 0xE0)
    {
        ExtendedKey = true;
        return;
    }
    if (ExtendedKey)
    {
        uint8_t Code = ScanCode & 0x7FU;
        bool Released = (ScanCode & 0x80U) != 0;
        ExtendedKey = false;
        if (Released) {
            if (Code == LastScanCode) LastScanCode = 0;
            return;
        }
        
        uint64_t now = PITGetMilliseconds();
        if (Code == LastScanCode) {
            if (now - KeyHoldStartTime < 500 || now - LastRepeatTime < 35) {
                return;
            }
            LastRepeatTime = now;
        } else {
            LastScanCode = Code;
            KeyHoldStartTime = now;
            LastRepeatTime = now;
        }

        if (Code == 0x48) KbdBufferPush(KBD_KEY_UP);
        else if (Code == 0x50) KbdBufferPush(KBD_KEY_DOWN);
        else if (Code == 0x4B) KbdBufferPush(KBD_KEY_LEFT);
        else if (Code == 0x4D) KbdBufferPush(KBD_KEY_RIGHT);
        else if (Code == 0x47) KbdBufferPush(KBD_KEY_HOME);
        else if (Code == 0x4F) KbdBufferPush(KBD_KEY_END);
        else if (Code == 0x53) KbdBufferPush(KBD_KEY_DELETE);
        else if (Code == 0x49) KbdBufferPush(KBD_KEY_PAGE_UP);
        else if (Code == 0x51) KbdBufferPush(KBD_KEY_PAGE_DN);
        else if (Code == 0x52) KbdBufferPush(KBD_KEY_INSERT);
        return;
    }

    /*
     * Key release
     */
    if (ScanCode & 0x80)
    {
        uint8_t ReleasedCode = ScanCode & 0x7F;

        KeyPressed[ReleasedCode] = false;
        if (ReleasedCode == LastScanCode)
        {
            LastScanCode = 0;
        }

        if (ReleasedCode == 0x2A || ReleasedCode == 0x36)
        {
            KbdState.Shift = false;
        }

        if (ReleasedCode == 0x1D)
        {
            KbdState.Ctrl = false;
        }

        return;
    }

    /*
     * Typematic Key Repeat Handling
     */
    uint64_t now = PITGetMilliseconds();
    if (KeyPressed[ScanCode])
    {
        if (ScanCode == LastScanCode)
        {
            if (now - KeyHoldStartTime < 500 || now - LastRepeatTime < 35)
            {
                return;
            }
            LastRepeatTime = now;
        }
        else
        {
            return;
        }
    }
    else
    {
        KeyPressed[ScanCode] = true;
        LastScanCode = ScanCode;
        KeyHoldStartTime = now;
        LastRepeatTime = now;
    }

    if (ScanCode == 0x2A || ScanCode == 0x36)
    {
        KbdState.Shift = true;
        return;
    }

    if (ScanCode == 0x1D)
    {
        KbdState.Ctrl = true;
        return;
    }

    if (ScanCode == 0x3A)
    {
        KbdState.CapsLock = !KbdState.CapsLock;
        return;
    }

    char Character = KeyboardLayoutNormal[ScanCode];
    if (Character == 0)
    {
        return;
    }

    if (Character >= 'a' && Character <= 'z')
    {
        if (KbdState.Shift ^ KbdState.CapsLock)
        {
            Character = KeyboardLayoutShift[ScanCode];
        }
    }
    else
    {
        if (KbdState.Shift)
        {
            Character = KeyboardLayoutShift[ScanCode];
        }
    }

    KbdBufferPush((uint8_t)Character);
}
