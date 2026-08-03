#include <stdint.h>
#include "string.h"

void strcpy(char *dst, const char *src)
{
    while(*src)
    {
        *dst++ = *src++;
    }

    *dst = '\0';
}

void strncpy(char *dst, const char *src, uint64_t n)
{
    uint64_t i = 0;
    while (i < n && src[i] != '\0')
    {
        dst[i] = src[i];
        i++;
    }
    while (i < n)
    {
        dst[i] = '\0';
        i++;
    }
}

void memcpy(void *dst, const void *src, uint64_t size)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    for(uint64_t i=0; i<size; i++)
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

int strcmp(const char *a, const char *b)
{
    while(*a && (*a==*b))
    {
        a++;
        b++;
    }

    return *(unsigned char*)a - *(unsigned char*)b;
}

char tolower_c(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

char toupper_c(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - ('a' - 'A');
    return c;
}

int strcasecmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = tolower_c(*a);
        char cb = tolower_c(*b);
        if (ca != cb)
            return (unsigned char)ca - (unsigned char)cb;
        a++;
        b++;
    }
    return (unsigned char)tolower_c(*a) - (unsigned char)tolower_c(*b);
}

int strncasecmp(const char *a, const char *b, uint64_t n)
{
    for (uint64_t i = 0; i < n; i++)
    {
        if (a[i] == '\0' || b[i] == '\0')
            return (unsigned char)tolower_c(a[i]) - (unsigned char)tolower_c(b[i]);
        char ca = tolower_c(a[i]);
        char cb = tolower_c(b[i]);
        if (ca != cb)
            return (unsigned char)ca - (unsigned char)cb;
    }
    return 0;
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

