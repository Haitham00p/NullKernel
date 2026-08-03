#include "include/kshell.h"
#include "include/kstring.h"
#include "include/kchar.h"

bool ShellArgToUInt64(const char *Arg, uint64_t *OutValue)
{
    if (Arg == NULL || OutValue == NULL)
    {
        return false;
    }

    const char *Ptr = Arg;
    while (IsSpace(*Ptr))
    {
        Ptr++;
    }

    if (*Ptr == '+')
    {
        Ptr++;
    }

    if (!IsDigit(*Ptr))
    {
        return false;
    }

    uint64_t Value = StringToUInt64(Arg);

    while (IsDigit(*Ptr))
    {
        Ptr++;
    }

    while (IsSpace(*Ptr))
    {
        Ptr++;
    }

    if (*Ptr != '\0')
    {
        return false;
    }

    *OutValue = Value;
    return true;
}

bool ShellArgToInt64(const char *Arg, int64_t *OutValue)
{
    if (Arg == NULL || OutValue == NULL)
    {
        return false;
    }

    const char *Ptr = Arg;
    while (IsSpace(*Ptr))
    {
        Ptr++;
    }

    if (*Ptr == '-' || *Ptr == '+')
    {
        Ptr++;
    }

    if (!IsDigit(*Ptr))
    {
        return false;
    }

    int64_t Value = StringToInt64(Arg);

    while (IsDigit(*Ptr))
    {
        Ptr++;
    }

    while (IsSpace(*Ptr))
    {
        Ptr++;
    }

    if (*Ptr != '\0')
    {
        return false;
    }

    *OutValue = Value;
    return true;
}
