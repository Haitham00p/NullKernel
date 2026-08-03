#include "include/kterminal.h"
#include "include/kstring.h"
#include "kernel/terminal/terminal.h"

void TerminalWriteUInt64(uint64_t Value, uint32_t Color)
{
    char Buffer[32];
    UInt64ToString(Value, Buffer);
    TerminalWrite32(Buffer, Color);
}

void TerminalWriteInt64(int64_t Value, uint32_t Color)
{
    char Buffer[32];
    Int64ToString(Value, Buffer);
    TerminalWrite32(Buffer, Color);
}

void TerminalWriteHex64(uint64_t Value, uint32_t Color)
{
    char Buffer[32];
    UInt64ToHexString(Value, Buffer);
    TerminalWrite32(Buffer, Color);
}

void TerminalWritePointer(const void *Ptr, uint32_t Color)
{
    char Buffer[32];
    UInt64ToHexString((uintptr_t)Ptr, Buffer);
    TerminalWrite32(Buffer, Color);
}
