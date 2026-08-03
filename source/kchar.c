#include "include/kchar.h"

bool IsDigit(char C)
{
    return (C >= '0' && C <= '9');
}

bool IsAlpha(char C)
{
    return ((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z'));
}

bool IsHexDigit(char C)
{
    return (IsDigit(C) || (C >= 'a' && C <= 'f') || (C >= 'A' && C <= 'F'));
}

bool IsSpace(char C)
{
    return (C == ' ' || C == '\t' || C == '\n' || C == '\r' || C == '\f' || C == '\v');
}
