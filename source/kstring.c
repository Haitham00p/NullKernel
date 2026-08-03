#include "include/kstring.h"
#include "include/kchar.h"

/* ========================================================================= */
/* String Comparison Helpers                                                */
/* ========================================================================= */

int StringCompare(const char *A, const char *B)
{
    if (A == B)
    {
        return 0;
    }
    if (A == NULL)
    {
        return -1;
    }
    if (B == NULL)
    {
        return 1;
    }

    while (*A != '\0' && *A == *B)
    {
        A++;
        B++;
    }

    return (int)(unsigned char)*A - (int)(unsigned char)*B;
}

bool StringEquals(const char *A, const char *B)
{
    return (StringCompare(A, B) == 0);
}

bool StringStartsWith(const char *String, const char *Prefix)
{
    if (Prefix == NULL)
    {
        return true;
    }
    if (String == NULL)
    {
        return false;
    }

    while (*Prefix != '\0')
    {
        if (*String != *Prefix)
        {
            return false;
        }
        String++;
        Prefix++;
    }

    return true;
}

bool StringEndsWith(const char *String, const char *Suffix)
{
    if (Suffix == NULL)
    {
        return true;
    }
    if (String == NULL)
    {
        return false;
    }

    uint64_t StrLen = 0;
    while (String[StrLen] != '\0')
    {
        StrLen++;
    }

    uint64_t SuffixLen = 0;
    while (Suffix[SuffixLen] != '\0')
    {
        SuffixLen++;
    }

    if (SuffixLen > StrLen)
    {
        return false;
    }

    return StringEquals(String + (StrLen - SuffixLen), Suffix);
}

/* ========================================================================= */
/* String to Number Conversions                                             */
/* ========================================================================= */

uint64_t StringToUInt64(const char *Str)
{
    if (Str == NULL)
    {
        return 0;
    }

    while (IsSpace(*Str))
    {
        Str++;
    }

    if (*Str == '+')
    {
        Str++;
    }

    uint64_t Result = 0;
    while (IsDigit(*Str))
    {
        Result = Result * 10ULL + (uint64_t)(*Str - '0');
        Str++;
    }

    return Result;
}

int64_t StringToInt64(const char *Str)
{
    if (Str == NULL)
    {
        return 0;
    }

    while (IsSpace(*Str))
    {
        Str++;
    }

    bool Negative = false;
    if (*Str == '-')
    {
        Negative = true;
        Str++;
    }
    else if (*Str == '+')
    {
        Str++;
    }

    uint64_t Result = 0;
    while (IsDigit(*Str))
    {
        Result = Result * 10ULL + (uint64_t)(*Str - '0');
        Str++;
    }

    if (Negative)
    {
        return -(int64_t)Result;
    }
    return (int64_t)Result;
}

/* ========================================================================= */
/* Number to String Conversions                                             */
/* ========================================================================= */

char *UInt64ToString(uint64_t Value, char *Buffer)
{
    if (Buffer == NULL)
    {
        return NULL;
    }

    if (Value == 0)
    {
        Buffer[0] = '0';
        Buffer[1] = '\0';
        return Buffer;
    }

    char Temp[32];
    int Index = 0;

    while (Value > 0)
    {
        Temp[Index++] = (char)('0' + (Value % 10ULL));
        Value /= 10ULL;
    }

    int BufIdx = 0;
    while (Index > 0)
    {
        Buffer[BufIdx++] = Temp[--Index];
    }
    Buffer[BufIdx] = '\0';

    return Buffer;
}

char *Int64ToString(int64_t Value, char *Buffer)
{
    if (Buffer == NULL)
    {
        return NULL;
    }

    if (Value < 0)
    {
        Buffer[0] = '-';
        uint64_t Pos = (uint64_t)(-(Value + 1)) + 1ULL;
        UInt64ToString(Pos, Buffer + 1);
        return Buffer;
    }

    return UInt64ToString((uint64_t)Value, Buffer);
}

char *UInt64ToHexString(uint64_t Value, char *Buffer)
{
    if (Buffer == NULL)
    {
        return NULL;
    }

    Buffer[0] = '0';
    Buffer[1] = 'x';

    if (Value == 0)
    {
        Buffer[2] = '0';
        Buffer[3] = '\0';
        return Buffer;
    }

    static const char HexDigits[] = "0123456789ABCDEF";
    char Temp[32];
    int Index = 0;

    while (Value > 0)
    {
        Temp[Index++] = HexDigits[Value & 0xFULL];
        Value >>= 4;
    }

    int BufIdx = 2;
    while (Index > 0)
    {
        Buffer[BufIdx++] = Temp[--Index];
    }
    Buffer[BufIdx] = '\0';

    return Buffer;
}
