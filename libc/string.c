/* Copyright (C) 2026 Haitham Al-Nasser
 * This file is part of NullOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NullOS. If not, see <https://www.gnu.org/licenses/>.
 */

#include "libc/string.h"
#include "libc/stdlib.h"

void *memmove(void *dst, const void *src, size_t n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;

    if (d < s)
    {
        for (size_t i = 0; i < n; i++)
        {
            d[i] = s[i];
        }
    }
    else if (d > s)
    {
        for (size_t i = n; i > 0; i--)
        {
            d[i - 1] = s[i - 1];
        }
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;

    for (size_t i = 0; i < n; i++)
    {
        if (pa[i] != pb[i])
        {
            return (int)pa[i] - (int)pb[i];
        }
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    char ch = (char)c;

    for (; *s != '\0'; s++)
    {
        if (*s == ch)
        {
            return (char *)s;
        }
    }
    return ch == '\0' ? (char *)s : NULL;
}

char *strrchr(const char *s, int c)
{
    char ch = (char)c;
    const char *found = NULL;

    for (; *s != '\0'; s++)
    {
        if (*s == ch)
        {
            found = s;
        }
    }
    if (ch == '\0')
    {
        return (char *)s;
    }
    return (char *)found;
}

char *strstr(const char *haystack, const char *needle)
{
    if (*needle == '\0')
    {
        return (char *)haystack;
    }

    for (; *haystack != '\0'; haystack++)
    {
        const char *h = haystack;
        const char *n = needle;

        while (*h != '\0' && *n != '\0' && *h == *n)
        {
            h++;
            n++;
        }
        if (*n == '\0')
        {
            return (char *)haystack;
        }
    }
    return NULL;
}

int strncmp(const char *a, const char *b, size_t n)
{
    if (n == 0)
    {
        return 0;
    }

    for (; n > 1 && *a != '\0' && *a == *b; a++, b++, n--)
    {
    }
    return (int)((unsigned char)*a - (unsigned char)*b);
}

char *strcat(char *dst, const char *src)
{
    char *d = dst;

    while (*d != '\0')
    {
        d++;
    }
    while ((*d++ = *src++) != '\0')
    {
    }
    return dst;
}

char *strncat(char *dst, const char *src, size_t n)
{
    char *d = dst;

    while (*d != '\0')
    {
        d++;
    }
    while (n > 0 && *src != '\0')
    {
        *d++ = *src++;
        n--;
    }
    *d = '\0';
    return dst;
}

char *strdup(const char *s)
{
    size_t len = strlen(s) + 1U;
    char *copy = (char *)malloc(len);

    if (copy != NULL)
    {
        memcpy(copy, s, len);
    }
    return copy;
}

static char *strtok_saved;

char *strtok(char *str, const char *delim)
{
    char *start;

    if (str != NULL)
    {
        strtok_saved = str;
    }
    if (strtok_saved == NULL)
    {
        return NULL;
    }

    while (*strtok_saved != '\0' && strchr(delim, *strtok_saved) != NULL)
    {
        strtok_saved++;
    }
    if (*strtok_saved == '\0')
    {
        strtok_saved = NULL;
        return NULL;
    }

    start = strtok_saved;
    while (*strtok_saved != '\0' && strchr(delim, *strtok_saved) == NULL)
    {
        strtok_saved++;
    }
    if (*strtok_saved != '\0')
    {
        *strtok_saved = '\0';
        strtok_saved++;
    }
    else
    {
        strtok_saved = NULL;
    }
    return start;
}

size_t strspn(const char *s, const char *accept)
{
    size_t count = 0;

    for (; *s != '\0' && strchr(accept, *s) != NULL; s++)
    {
        count++;
    }
    return count;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t count = 0;

    for (; *s != '\0' && strchr(reject, *s) == NULL; s++)
    {
        count++;
    }
    return count;
}

size_t strnlen(const char *s, size_t maxlen)
{
    size_t i = 0;

    for (; i < maxlen && s[i] != '\0'; i++)
    {
    }
    return i;
}

size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = strlen(src);

    if (size > 0)
    {
        size_t n = len < size - 1U ? len : size - 1U;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return len;
}
