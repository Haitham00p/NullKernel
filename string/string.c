#include <stdint.h>
#include "../string/string.h"

void strcpy(char *dst,const char *src)
{
    while(*src)
    {
        *dst++ = *src++;
    }

    *dst = '\0';
}



void memcpy(void *dst,const void *src,uint64_t size)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    for(uint64_t i=0;i<size;i++)
    {
        d[i]=s[i];
    }
}



uint64_t strlen(const char *str)
{
    uint64_t len=0;

    while(str[len])
        len++;

    return len;
}

void *memset(void *dest, int value, uint64_t size)
{
    uint8_t *ptr = (uint8_t *)dest;

    for (uint64_t i = 0; i < size; i++)
    {
        ptr[i] = (uint8_t)value;
    }

    return dest;
}

int strcmp(const char *a,const char *b)
{
    while(*a && (*a==*b))
    {
        a++;
        b++;
    }

    return *(unsigned char*)a -
           *(unsigned char*)b;
}

uint32_t StringHexToUInt32(const char *String, uint32_t *Value)
{
    *Value = 0;

    if (String[0] == '0' &&
        (String[1] == 'x' || String[1] == 'X'))
    {
        String += 2;
    }

    while (*String)
    {
        char c = *String;
        uint32_t Digit;

        if (c >= '0' && c <= '9')
            Digit = c - '0';

        else if (c >= 'a' && c <= 'f')
            Digit = c - 'a' + 10;

        else if (c >= 'A' && c <= 'F')
            Digit = c - 'A' + 10;

        else
            return 0;       

        *Value = (*Value << 4) | Digit;
        String++;
    }

    return 1;                 
}