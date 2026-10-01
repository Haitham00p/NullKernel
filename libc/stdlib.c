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

#include "libc/stdlib.h"
#include "libc/ctype.h"
#include "lib/string/string.h"
#include "mm/heap/heap.h"

void *malloc(size_t size)
{
    return kmalloc(size);
}

void free(void *ptr)
{
    kfree(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = malloc(total);

    if (ptr != NULL)
    {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    void *next;

    if (ptr == NULL)
    {
        return malloc(size);
    }
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    next = malloc(size);
    if (next == NULL)
    {
        return NULL;
    }
    memcpy(next, ptr, size);
    free(ptr);
    return next;
}

int atoi(const char *s)
{
    return (int)strtol(s, NULL, 10);
}

long atol(const char *s)
{
    return strtol(s, NULL, 10);
}

unsigned long strtoul(const char *s, char **endptr, int base)
{
    unsigned long acc = 0;
    int neg = 0;
    int digit;
    unsigned long cutoff;
    int cutlim;

    while (isspace((int)*s))
    {
        s++;
    }

    if (*s == '-')
    {
        neg = 1;
        s++;
    }
    else if (*s == '+')
    {
        s++;
    }

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        base = 16;
        s += 2;
    }
    else if (base == 0)
    {
        base = (*s == '0') ? 8 : 10;
    }
    if (base < 2 || base > 36)
    {
        if (endptr != NULL)
        {
            *endptr = (char *)s;
        }
        return 0;
    }

    cutoff = 0xFFFFFFFFFFFFFFFFUL;
    cutlim = 0;

    while (1)
    {
        char c = *s;

        if (isdigit((int)c))
        {
            digit = c - '0';
        }
        else if (isalpha((int)c))
        {
            digit = tolower((int)c) - 'a' + 10;
        }
        else
        {
            break;
        }
        if (digit >= base)
        {
            break;
        }

        if (acc > cutoff || (acc == cutoff && (unsigned int)digit > (unsigned int)cutlim))
        {
            acc = 0xFFFFFFFFFFFFFFFFUL;
        }
        else
        {
            acc = acc * (unsigned long)base + (unsigned long)digit;
        }
        s++;
    }

    if (endptr != NULL)
    {
        *endptr = (char *)s;
    }
    return neg ? (unsigned long)(-(long)acc) : acc;
}

long strtol(const char *s, char **endptr, int base)
{
    unsigned long val = strtoul(s, endptr, base);
    const char *p = s;
    int neg = 0;

    while (isspace((int)*p))
    {
        p++;
    }
    if (*p == '-')
    {
        neg = 1;
        p++;
    }
    else if (*p == '+')
    {
        p++;
    }
    if (neg)
    {
        if ((long)val < 0)
        {
            return (long)val;
        }
        return 0x8000000000000000L;
    }
    if ((long)val > 0)
    {
        return (long)val;
    }
    return 0x7FFFFFFFFFFFFFFFL;
}

int abs(int x)
{
    return x < 0 ? -x : x;
}

long labs(long x)
{
    return x < 0 ? -x : x;
}

static char *num_to_string(unsigned long value, char *buf, int base, int uppercase)
{
    static const char digits_lower[] = "0123456789abcdef";
    static const char digits_upper[] = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_upper : digits_lower;
    char tmp[68];
    int i = 0;
    int j = 0;

    if (base < 2 || base > 36)
    {
        base = 10;
    }
    do
    {
        tmp[i++] = digits[value % (unsigned long)base];
        value /= (unsigned long)base;
    } while (value > 0);

    while (i > 0)
    {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
    return buf;
}

char *itoa(int value, char *buf, int base)
{
    char *p = buf;

    if (base == 10 && value < 0)
    {
        *p++ = '-';
        return num_to_string((unsigned long)(-(long)value), p, base, 0);
    }
    return num_to_string((unsigned long)value, p, base, 0);
}

char *ltoa(long value, char *buf, int base)
{
    char *p = buf;

    if (base == 10 && value < 0)
    {
        *p++ = '-';
        return num_to_string((unsigned long)(-(long long)value), p, base, 0);
    }
    return num_to_string((unsigned long)value, p, base, 0);
}

char *utoa(unsigned int value, char *buf, int base)
{
    return num_to_string((unsigned long)value, buf, base, 0);
}

char *ultoa(unsigned long value, char *buf, int base)
{
    return num_to_string(value, buf, base, 0);
}

static unsigned int rand_state = 1;

void srand(unsigned int seed)
{
    rand_state = seed != 0 ? seed : 1;
}

int rand(void)
{
    rand_state = rand_state * 1103515245U + 12345U;
    return (int)((rand_state >> 16) & 0x7FFFFFFFU);
}

#ifndef __NCC__
void qsort(void *base, size_t nmemb, size_t size,
           int (*compare)(const void *, const void *))
{
    char *arr = (char *)base;

    if (nmemb < 2)
    {
        return;
    }

    if (size == 1U)
    {
        size_t i, j;
        for (i = 1; i < nmemb; i++)
        {
            char key = arr[i];
            j = i;
            while (j > 0 && compare(&arr[j - 1], &key) > 0)
            {
                arr[j] = arr[j - 1];
                j--;
            }
            arr[j] = key;
        }
        return;
    }

    {
        size_t i, j;
        char *key = malloc(size);
        char *tmp = malloc(size);

        if (key == NULL || tmp == NULL)
        {
            free(key);
            free(tmp);
            return;
        }
        for (i = 1; i < nmemb; i++)
        {
            memcpy(key, arr + i * size, size);
            j = i;
            while (j > 0 && compare(arr + (j - 1) * size, key) > 0)
            {
                memcpy(arr + j * size, arr + (j - 1) * size, size);
                j--;
            }
            memcpy(arr + j * size, key, size);
        }
        free(key);
        free(tmp);
    }
}
#endif
